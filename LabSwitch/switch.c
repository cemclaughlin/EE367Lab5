/*
* host.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#include "main.h"
#include "net.h"
#include "host.h"
#include "packet.h"
#include "switch.h"

#define MAX_ROUTING_TABLE_SIZE 20
#define TENMILLISEC 10000   /* 10 millisecond sleep */


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

/*
*  Main
*/

void switch_main(int host_id)
{

    struct net_port *node_port_list;
    struct net_port **node_port;  // Array of pointers to node ports
    int node_port_num;            // Number of node ports

    int i, k, n;
    int dst;
    int pid;

    FILE *fp;

    struct packet *in_packet; /* Incoming packet */
    struct packet *new_packet;
    struct treepacket *in_tree_packet; /*Incoming tree packet*/
    struct treepacket *out_tree_packet;

    struct net_port *p;
    struct switch_job *new_job;
    struct switch_job *new_job2;

    struct switch_job_queue job_q;
    struct switch_job_queue tree_q;

    struct switch_local_tree old_tree;
    struct switch_local_tree new_tree;

    //creating and initializing routing table
    struct routing_table_entry routing_table[MAX_ROUTING_TABLE_SIZE];
    routing_table_init(routing_table);

    /*
* Create an array node_port[ ] to store the network link ports
* at the host.  The number of ports is node_port_num
*/
    node_port_list = net_get_port_list(host_id);

    /*  Count the number of network link ports */
    node_port_num = 0;
    for (p=node_port_list; p!=NULL; p=p->next) {
        node_port_num++;
    }
    /* Create memory space for the array */
    node_port = (struct net_port **)
                malloc(node_port_num*sizeof(struct net_port *));

    /* Load ports into the array */
    p = node_port_list;
    for (k = 0; k < node_port_num; k++) {
        node_port[k] = p;
        p = p->next;
    }

    /* Initialize the job queue */
    switch_job_q_init(&job_q);

    //printf("\th%d: online\n", host_id);

    while(1) {


        /*
   * Get packets from incoming links and translate to jobs
   * Put jobs in job queue
   */
        if((pid=fork()) == 0){
          //TREEEEEEES
          ///////////////////////////////////////////
          /////Add local tree info initilization ////
          new_tree.localRootID = host_id;
          new_tree.localRootDist = 0;
          new_tree.localParent = NULL;
          //new_tree.localPortTree[] = ;

          //send initial information


          //read replys

          //compare/update as necessary

          //if changed, resend information

          //after each send, create a TTL job?

          //end?
        }

        for (k = 0; k < node_port_num; k++) { /* Scan all ports */

            in_packet = (struct packet *) malloc(sizeof(struct packet));
            n = packet_recv(node_port[k], in_packet);

            if (n > 0)  { //changed to simple read if there is a packet

                new_job = (struct switch_job *)
                          malloc(sizeof(struct switch_job));
                new_job->in_port_index = k;
                new_job->packet = in_packet;

                if(packet->type == PKT_TREE){
                  switch_job_q_add(&tree_q, new_job);
                }

                //if in_port_index is not in routing_table, add it in
                else {
                    if( (i = find_routing_entry(routing_table,in_packet->src)) < 0) {
                      add_routing_entry(routing_table, in_packet->src ,k);
                    }
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

            //printf("\th%d: jobs available\n", host_id);

            /* Get a new job from the job queue */
            new_job = switch_job_q_remove(&job_q);

            //printf("\th%d: got a job\n", host_id);


            //if host id is found -> send to host port
            if((k=find_routing_entry(routing_table,in_packet->src)) >=0 ) {
              packet_send(node_port[k],new_job->packet);
            }
            else {//otherwise
                //send to all except incoming port
                for (k=0; k<node_port_num; k++) {
                  if(k!=new_job->in_port_index)
                    packet_send(node_port[k], new_job->packet);
                }
            }
            free(new_job->packet);
            free(new_job);


            //printf("\th%d: switch out \n", host_id);
        }

        /* The host goes to sleep for 10 ms */
        usleep(TENMILLISEC);

      wait(); //may cause problems OTL

    } /* End of while loop */

}
