/*
* host.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "main.h"
#include "net.h"
#include "host.h"
#include "packet.h"
#include "switch.h"

#define MAX_ROUTING_TABLE_SIZE 20
#define TENMILLISEC 10000   /* 10 millisecond sleep */
#define TREE_TTL 20


/* Job queue operations */

/* Add a job to the job queue */
void switch_job_q_add(struct switch_job_queue *j_q, struct switch_job *j)
{
    //printf("\t\tadd: j=%p\n", j);

    j->next = NULL;
    if (j_q->head == NULL ) {
        j_q->head = j;
        j_q->tail = j;
        j_q->occ = 1;
    }
    else {
        (j_q->tail)->next = j;
        j_q->tail = j;
        j_q->occ++;
    }

    //printf("\t\tadd: end\n");
}

/* Remove job from the job queue, and return pointer to the job*/
struct switch_job *switch_job_q_remove(struct switch_job_queue *j_q)
{
    struct switch_job *j;

    if (j_q->occ == 0)
    {
        //printf("\t\trem: NULL\n");
        return(NULL);
    }
    j = j_q->head;
    //printf("\t\trem: j=%p\n", j);
    j_q->head = (j_q->head)->next;
    //printf("\t\trem: j=%p *\n", j_q->head);
    j_q->occ--;

    //printf("\t\trem: END\n");

    return(j);
}

/* Initialize job queue */
void switch_job_q_init(struct switch_job_queue *j_q)
{
    j_q->occ = 0;
    j_q->head = NULL;
    j_q->tail = NULL;
}

int switch_job_q_num(struct switch_job_queue *j_q)
{
    return j_q->occ;
}

/*
*   routing table functions
*/
void routing_table_init(struct routing_table_entry rt[]){
  for(int i=0;i<MAX_ROUTING_TABLE_SIZE;i++){
    rt[i].key = '\03';
  }
}


int find_routing_entry(struct routing_table_entry rt[], char key){
  for (int i=0; i<MAX_ROUTING_TABLE_SIZE && rt[i].key!='\03' ;i++){
      if(rt[i].key==key)return i;
  }
  return -1;
}

void add_routing_entry(struct routing_table_entry rt[], char key, int num){
  int i;
  for (i = 0; i<MAX_ROUTING_TABLE_SIZE && rt[i].key!='\03';i++)
  if(i<MAX_ROUTING_TABLE_SIZE){
    rt[i].key=key;
    rt[i].port_num=num;
  }
  return;
}

void *treeCreation(void *vargp){
  struct thread_arg * threadArgs = vargp;
  struct packet * newPacket;
  struct switch_job * new_tree_job;
  struct switch_job * read_job;
  int temp_index;

  while(1){
    //init tree data for a tree creation session
    threadArgs->tree_struct->localRootID = threadArgs->switch_id;
    threadArgs->tree_struct->localRootDist = 0;
    threadArgs->tree_struct->localParent = '\04';
    threadArgs->finalize = 0;

    //make packet for tree discovery job
    newPacket = (struct packet *) malloc(sizeof(struct packet));
    newPacket->src = threadArgs->tree_struct->localRootID;
    newPacket->dst = (char) threadArgs->tree_struct->localRootDist;
    newPacket->type = (char) PKT_TREE;
    newPacket->length = 2;
    newPacket->payload[0]='H';
    newPacket->payload[1]='N';

    //attach packet to job and add to job_q
    new_tree_job = (struct switch_job *) malloc(sizeof(struct switch_job));
    new_tree_job->packet = newPacket;
    new_tree_job->in_port_index = -1;
    new_tree_job->type = JOB_TREE_SEND;
    switch_job_q_add(threadArgs->job_q, new_tree_job);

    //init done, creating main tree creation loop
    while(threadArgs->finalize==0){
      //if there's a new tree packet to read
      if (switch_job_q_num(threadArgs->tree_q) > 0){
        read_job = switch_job_q_remove(threadArgs->tree_q);
        temp_index = read_job->in_port_index;

        //if new root_id is lower OR (new_root_id is same and new_dist is lower)
        if(read_job->packet->src < threadArgs->tree_struct->localRootID ||
          (read_job->packet->src == threadArgs->tree_struct->localRootID &&
            (int) read_job->packet->dst < threadArgs->tree_struct->localRootDist - 1)){
          //update tree information
          threadArgs->tree_struct->localRootID = read_job->packet->src;
          threadArgs->tree_struct->localRootDist = (int) read_job->packet->dst + 1;
          threadArgs->tree_struct->localParent = temp_index;

          //reset inTree flags
          for( int t = 0; t < threadArgs->num_ports; t++){
            if(t == temp_index) threadArgs->inTree[t] = 1;  //setting the parent
            else threadArgs->inTree[t] = 0;
          }

          //make packet for tree discovery job
          newPacket = (struct packet *) malloc(sizeof(struct packet));
          newPacket->src = threadArgs->tree_struct->localRootID;
          newPacket->dst = (char) threadArgs->tree_struct->localRootDist;
          newPacket->type = (char) PKT_TREE;
          newPacket->length = 2;
          newPacket->payload[0]='S';
          newPacket->payload[1]='N';

          //attach packet to job and add to job_q
          new_tree_job = (struct switch_job *) malloc(sizeof(struct switch_job));
          new_tree_job->packet = newPacket;
          new_tree_job->in_port_index = temp_index;
          new_tree_job->type = JOB_TREE_SEND;
          switch_job_q_add(threadArgs->job_q, new_tree_job);
        }
        //if the sender is child, or if type='H' add to inTree
        if(read_job->packet->payload[0] == 'H' || read_job->packet->payload[1] == 'Y'){
          threadArgs->inTree[temp_index]=1;
        }


      }
    }
    //sleep
    usleep(TENMILLISEC);
  }

}

/*
*  Main
*/

void switch_main(int switch_id)
{

    struct net_port *node_port_list;
    struct net_port **node_port;  // Array of pointers to node ports
    int node_port_num;            // Number of node ports

    int i, k, n;
    int dst;
    int pid;
    int tree_ttl; //for JOB_TREE_WAITING time-to-live
    int * switch_in_tree;
    int * thread_in_tree;
    int finalize;

    FILE *fp;

    struct packet *in_packet; /* Incoming packet */
    struct packet *new_packet;
    //struct treepacket *in_tree_packet; /*Incoming tree packet*/
    //struct treepacket *out_tree_packet;

    struct net_port *p;
    struct switch_job *new_job;
    struct switch_job *new_job2;

    struct switch_job_queue job_q;
    struct switch_job_queue tree_q;

    struct switch_local_tree switch_tree;
    struct switch_local_tree thread_tree;
    struct thread_arg * thread_args;
    pthread_t tid;

    //creating and initializing routing table
    struct routing_table_entry routing_table[MAX_ROUTING_TABLE_SIZE];
    routing_table_init(routing_table);

    /*
* Create an array node_port[ ] to store the network link ports
* at the host.  The number of ports is node_port_num
*/
    node_port_list = net_get_port_list(switch_id);

    /*  Count the number of network link ports */
    node_port_num = 0;
    for (p=node_port_list; p!=NULL; p=p->next) {
        node_port_num++;
    }
    /* Create memory space for the array */
    node_port = (struct net_port **)
                malloc(node_port_num*sizeof(struct net_port *));

    switch_in_tree = (int *) malloc(node_port_num*sizeof(int));
    thread_in_tree = (int *) malloc(node_port_num*sizeof(int));
    /* Load ports into the array */ //also init in_tree arrays
    p = node_port_list;
    for (k = 0; k < node_port_num; k++) {
        node_port[k] = p;
        switch_in_tree[k] = 0;
        thread_in_tree[k] = 0;
        p = p->next;
    }

    /* Initialize the job queue */
    switch_job_q_init(&job_q);
    switch_job_q_init(&tree_q);

    thread_args = (struct thread_arg *)malloc(sizeof(struct thread_arg));
    thread_args->tree_struct = &thread_tree;
    thread_args->inTree = thread_in_tree;
    thread_args->num_ports = node_port_num;
    thread_args->switch_id = switch_id;
    thread_args->job_q = &job_q;
    thread_args->tree_q = &tree_q;
    thread_args->finalize = &finalize;

    pthread_create(&tid, NULL, treeCreation, (void *)thread_args);

    //printf("\th%d: online\n", switch_id);

    while(1) {


        /*
   * Get packets from incoming links and translate to jobs
   * Put jobs in job queue
   */


        for (k = 0; k < node_port_num; k++) { /* Scan all ports */

            in_packet = (struct packet *) malloc(sizeof(struct packet));
            n = packet_recv(node_port[k], in_packet);

            if (n > 0)  { //changed to simple read if there is a packet

                new_job = (struct switch_job *)
                          malloc(sizeof(struct switch_job));
                new_job->in_port_index = k;
                new_job->packet = in_packet;

                if(in_packet->type == PKT_TREE){
                  switch_job_q_add(&tree_q, new_job);
                }

                //if in_port_index is not in routing_table, add it in
                else {
                    if( (i = find_routing_entry(routing_table,in_packet->src)) < 0) {
                      add_routing_entry(routing_table, in_packet->src ,k);
                    }
                    new_job->type = JOB_FORWARD_PACKET;
                    switch_job_q_add(&job_q, new_job);
                }
            }
            else {  //remove this?
                free(in_packet);
            }
        }

        /*
   * Execute one job in the job queue
   */

        if (switch_job_q_num(&job_q) > 0) {

            //printf("\th%d: jobs available\n", switch_id);

            /* Get a new job from the job queue */
            new_job = switch_job_q_remove(&job_q);

            //printf("\th%d: got a job\n", switch_id);

            if(new_job->type == JOB_FORWARD_PACKET){  //only ports in_tree
              //if host id is found -> send to host port
              if((k=find_routing_entry(routing_table,in_packet->src)) >=0 && switch_in_tree[k] == 1) {
                packet_send(node_port[k],new_job->packet);
              }
              else {//otherwise
                //send to all except incoming port
                for (k=0; k<node_port_num; k++) {
                  if(k!=new_job->in_port_index && switch_in_tree[k] == 1)
                  packet_send(node_port[k], new_job->packet);
                }
              }
              free(new_job->packet);
              free(new_job);
            }

            //send on all ports, reset tree_ttl
            if(new_job->type == JOB_TREE_SEND){
              for(k=0; k<node_port_num; k++){
                packet_send(node_port[k], new_job->packet);
              }
              free(new_job->packet);
              free(new_job);
              new_job = (struct switch_job *)malloc(sizeof(struct switch_job));
              tree_ttl = TREE_TTL;
              new_job->type = JOB_TREE_WAITING;
              switch_job_q_add(&job_q, new_job);
            }

            //check ttl
            if(new_job->type == JOB_TREE_WAITING){
              if(tree_ttl<=1){  //if dieing aka TimeToLive is over
                finalize = 1;   //flag thread to sleep
                //move tree data from thread to switch
                switch_tree.localRootID = thread_tree.localRootID;
                switch_tree.localRootDist = thread_tree.localRootDist;
                switch_tree.localParent = thread_tree.localParent;
                for(k = 0; k < node_port_num; k++){//copy in_tree array
                  switch_in_tree[k] = thread_in_tree[k];
                }
              }
              else{ //not time to die
                tree_ttl--;  //lower ttl counter
                switch_job_q_add(&job_q, new_job);  //recycle job
              }
            }
            //printf("\th%d: switch out \n", switch_id);
        }

        /* The switch goes to sleep for 10 ms */
        usleep(TENMILLISEC);

      //wait(); //may cause problems OTL

    } /* End of while loop */

}
