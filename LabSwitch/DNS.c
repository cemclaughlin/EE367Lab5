/*
* DNS.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "main.h"
#include "net.h"
#include "host.h"
#include "packet.h"
#include "DNS.h"

#define MAX_ROUTING_TABLE_SIZE 20
#define TENMILLISEC 10000   /* 10 millisecond sleep */
#define TREE_TTL 10


/* Job queue operations */

/* Add a job to the job queue */
void dns_job_q_add(struct dns_job_queue *j_q, struct dns_job *j)
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
struct dns_job *dns_job_q_remove(struct dns_job_queue *j_q)
{
    struct dns_job *j;

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
void dns_job_q_init(struct dns_job_queue *j_q)
{
    j_q->occ = 0;
    j_q->head = NULL;
    j_q->tail = NULL;
}

int dns_job_q_num(struct dns_job_queue *j_q)
{
    return j_q->occ;
}

/*
*   routing table functions
*/
void dns_routing_table_init(struct dns_routing_table_entry rt[]){
  for(int i=0;i<MAX_ROUTING_TABLE_SIZE;i++){
    rt[i].key = '\03';
  }
}


int dns_find_routing_entry(struct dns_routing_table_entry rt[], char key){
  for (int i=0; i<MAX_ROUTING_TABLE_SIZE && rt[i].key!='\03' ;i++){
      if(rt[i].key==key)return i;
  }
  return -1;
}

void dns_add_routing_entry(struct dns_routing_table_entry rt[], char key, int num){
  int i;
  for (i = 0; i<MAX_ROUTING_TABLE_SIZE && rt[i].key!='\03';i++)
  if(i<MAX_ROUTING_TABLE_SIZE){
    rt[i].key=key;
    rt[i].port_num=num;
  }
  return;
}

void dns_init(struct dns_entry dns_entry[]){
  for(int i =0; i <15; i++){
    dns_entry[i].name[0] = '\0';
  }
  return;
}

char dns_lookup(struct dns_entry dns_entry[], char name[]){
    for(int i =0; i <15; i++){
      if(!strcmp(dns_entry[i].name, name)){//if names match
        return dns_entry[i].key;
      }
    }
    return '\0';
}

int dns_add(struct dns_entry dns_entry[], char name[], char key){
  int i;
  for(i =0; i < 15 && dns_entry[i].name[0] != '\0'; i++){
    if(dns_entry[i].key == key){
      for(int j=0; name[j]!='\0' && j < 20;j++){
        dns_entry[i].name[j]=name[j];
      }
      return 1;
    }
  }
  if (i < 15){
    for(int j=0; name[j]!='\0' && j < 20;j++){
      dns_entry[i].name[j]=name[j];
    }
    return 1;
  }
  return -1;
}

/*
*  Main
*/

void dns_main(int dns_id)
{

    struct net_port *node_port_list;
    struct net_port **node_port;  // Array of pointers to node ports
    int node_port_num;            // Number of node ports

    int i, k, n;
    int dst;
    int pid;
    int tree_ttl; //for JOB_DNS_TREE_WAITING time-to-live
    int * dns_in_tree;
    int * temp_in_tree;
    int finalize;

    FILE *fp;

    struct packet *in_packet; /* Incoming packet */
    struct packet *new_packet;
    //struct treepacket *in_tree_packet; /*Incoming tree packet*/
    //struct treepacket *out_tree_packet;

    struct packet * child_new_packet;
    struct dns_job * new_tree_job;
    struct dns_job * read_job;

    struct net_port *p;
    struct dns_job *new_job;
    struct dns_job *new_job2;

    struct dns_job_queue job_q;
    struct dns_job_queue tree_q;

    struct dns_local_tree dns_tree;
    struct dns_local_tree temp_tree;
    //creating and initializing routing table
    struct dns_routing_table_entry routing_table[MAX_ROUTING_TABLE_SIZE];
    dns_routing_table_init(routing_table);

    char lookup_id;
    char register_name[20];

    struct dns_entry dns_table[15];

    dns_init(dns_table);

    /*
* Create an array node_port[ ] to store the network link ports
* at the host.  The number of ports is node_port_num
*/
    node_port_list = net_get_port_list(dns_id);

    /*  Count the number of network link ports */
    node_port_num = 0;
    for (p=node_port_list; p!=NULL; p=p->next) {
        node_port_num++;
    }
    /* Create memory space for the array */
    node_port = (struct net_port **)
                malloc(node_port_num*sizeof(struct net_port *));

    dns_in_tree = (int *) malloc(node_port_num*sizeof(int));
    temp_in_tree = (int *) malloc(node_port_num*sizeof(int));
    /* Load ports into the array */ //also init in_tree arrays
    p = node_port_list;
    for (k = 0; k < node_port_num; k++) {
        node_port[k] = p;
        dns_in_tree[k] = 0;
        temp_in_tree[k] = 0;
        p = p->next;
    }

    /* Initialize the job queue */
    dns_job_q_init(&job_q);
    dns_job_q_init(&tree_q);

    //tree creation
    if((pid=fork()) ==0){
      while(1){
        //init tree data for a tree creation session
        temp_tree.localRootID = dns_id;
        temp_tree.localRootDist = 0;
        temp_tree.localParent = '\0';
        finalize = 0;

        //make packet for tree discovery job
        child_new_packet = (struct packet *) malloc(sizeof(struct packet));
        child_new_packet->src = temp_tree.localRootID;
        child_new_packet->dst = (char) temp_tree.localRootDist;
        child_new_packet->type = (char) PKT_TREE;
        child_new_packet->length = 2;
        child_new_packet->payload[0]='S';
        child_new_packet->payload[1]='N';

        //attach packet to job and add to job_q
        new_tree_job = (struct dns_job *) malloc(sizeof(struct dns_job));
        new_tree_job->packet = child_new_packet;
        new_tree_job->type = JOB_DNS_TREE_SEND;
        dns_job_q_add(&job_q, new_tree_job);

        //init done, creating main tree creation loop
        while(finalize==0){
          //if there's a new tree packet to read
          if (dns_job_q_num(&tree_q) > 0){
            read_job = dns_job_q_remove(&tree_q);

            //if new root_id is lower OR (new_root_id is same and new_dist is lower)
            if(read_job->packet->src < temp_tree.localRootID ||
              (read_job->packet->src == temp_tree.localRootID &&
                (int) read_job->packet->dst < (temp_tree.localRootDist - 1))){
              //update tree information
              temp_tree.localRootID = read_job->packet->src;
              temp_tree.localRootDist = (int) read_job->packet->dst + 1;
              temp_tree.localParent = read_job->in_port_index;

              //reset inTree flags
              for( int t = 0; t < node_port_num; t++){
                if(t == read_job->in_port_index) temp_in_tree[t] = 1;  //setting the parent
                else temp_in_tree[t] = 0;
              }

              //make packet for tree discovery job
              child_new_packet = (struct packet *) malloc(sizeof(struct packet));
              child_new_packet->src = temp_tree.localRootID;
              child_new_packet->dst = (char) temp_tree.localRootDist;
              child_new_packet->type = (char) PKT_TREE;
              child_new_packet->length = 2;
              child_new_packet->payload[0]='S';
              child_new_packet->payload[1]='N';

              //attach packet to job and add to job_q
              new_tree_job = (struct dns_job *) malloc(sizeof(struct dns_job));
              new_tree_job->packet = child_new_packet;
              new_tree_job->in_port_index = read_job->in_port_index;
              new_tree_job->type = JOB_DNS_TREE_SEND;
              dns_job_q_add(&job_q, new_tree_job);
            }
            //if the sender is child, or if type='H' add to inTree
            if(read_job->packet->payload[0] == 'H' || read_job->packet->payload[1] == 'Y'){
              temp_in_tree[read_job->in_port_index]=1;
            }


          }
        }
        //sleep
        usleep(TENMILLISEC);
      }
    }

    //printf("\th%d: online\n", dns_id);

    while(1) {


        /*
   * Get packets from incoming links and translate to jobs
   * Put jobs in job queue
   */


        for (k = 0; k < node_port_num; k++) { /* Scan all ports */

            in_packet = (struct packet *) malloc(sizeof(struct packet));
            n = packet_recv(node_port[k], in_packet);

            if (n > 0)  { //changed to simple read if there is a packet

                new_job = (struct dns_job *)
                          malloc(sizeof(struct dns_job));
                new_job->in_port_index = k;
                new_job->packet = in_packet;

                if(in_packet->type == PKT_TREE){
                  dns_job_q_add(&tree_q, new_job);
                }

                //if in_port_index is not in routing_table, add it in
                else {
                    if( (i = dns_find_routing_entry(routing_table,in_packet->src)) < 0) {
                      dns_add_routing_entry(routing_table, in_packet->src ,k);
                    }
					          if(in_packet->type != DNS_REQ && in_packet->type != DNS_REGISTER){
						        new_job->type = JOB_DNS_FORWARD_PACKET;
						        dns_job_q_add(&job_q, new_job);
					          }
                    else if(in_packet->type == DNS_REQ){
                      if((lookup_id = dns_lookup(dns_table, in_packet->payload)) !='\0'){//if match is found
                          //create DNS_REPLY
                          new_packet = (struct packet *)
                                       malloc(sizeof(struct packet));
                          new_packet->dst = new_job->packet->src;
                          new_packet->src = (char) dns_id;
                          new_packet->type = DNS_REPLY;
                          new_packet->length = 1;
                          new_packet->payload[0] = lookup_id;

                          new_job2 = (struct dns_job *)
                                     malloc(sizeof(struct dns_job));
                          new_job2->type = JOB_DNS_FORWARD_PACKET;
                          new_job2->packet = new_packet;

                          /* Enter job in the job queue */
                          dns_job_q_add(&job_q, new_job2);
                      }
                      //if not found, do nothing, allow dns lookup to time out
                    }
                    else if(in_packet->type == DNS_REGISTER){
                        //read packet->payload for name
                        if(dns_add(dns_table, in_packet->payload, in_packet->src) <0){
                          //error message or something
                          printf("could not register name\n");
                        }
                    }
                }
            }
            else {  //remove this?
                free(in_packet);
            }
        }

        /*
   * Execute one job in the job queue
   */

        if (dns_job_q_num(&job_q) > 0) {

            //printf("\th%d: jobs available\n", dns_id);

            /* Get a new job from the job queue */
            new_job = dns_job_q_remove(&job_q);

            //printf("\th%d: got a job\n", dns_id);

            if(new_job->type == JOB_DNS_FORWARD_PACKET){  //only ports in_tree
              //if host id is found -> send to host port
              if((k=dns_find_routing_entry(routing_table,in_packet->src)) >=0 && dns_in_tree[k] == 1) {
                packet_send(node_port[k],new_job->packet);
              }
              else {//otherwise
                //send to all except incoming port
                for (k=0; k<node_port_num; k++) {
                  if(k!=new_job->in_port_index && dns_in_tree[k] == 1)
                  packet_send(node_port[k], new_job->packet);
                }
              }
              free(new_job->packet);
              free(new_job);
            }

            //send on all ports, reset tree_ttl
            if(new_job->type == JOB_DNS_TREE_SEND){
              for(k=0; k<node_port_num; k++){
                packet_send(node_port[k], new_job->packet);
              }
              free(new_job->packet);
              free(new_job);
              new_job = (struct dns_job *)malloc(sizeof(struct dns_job));
              tree_ttl = TREE_TTL;
              new_job->type = JOB_DNS_TREE_WAITING;
              dns_job_q_add(&job_q, new_job);
            }

            //check ttl
            if(new_job->type == JOB_DNS_TREE_WAITING){
              if(tree_ttl<=1){  //if dieing aka TimeToLive is over
                finalize = 1;   //flag thread to sleep
                //move tree data from thread to switch
                dns_tree.localRootID = temp_tree.localRootID;
                dns_tree.localRootDist = temp_tree.localRootDist;
                dns_tree.localParent = temp_tree.localParent;
                for(k = 0; k < node_port_num; k++){//copy in_tree array
                  dns_in_tree[k] = temp_in_tree[k];
                }
              }
              else{ //not time to die
                tree_ttl--;  //lower ttl counter
                dns_job_q_add(&job_q, new_job);  //recycle job
              }
            }
            //printf("\th%d: switch out \n", dns_id);
        }

        /* The switch goes to sleep for 10 ms */
        usleep(TENMILLISEC);

      //wait(); //may cause problems OTL

    } /* End of while loop */

}
