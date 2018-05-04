/*
 * DNS.h
 */

enum dns_job_type {
	JOB_DNS_FORWARD_PACKET,
	JOB_DNS_TREE_SEND,
	JOB_DNS_TREE_WAITING,
};

struct dns_job {
	enum dns_job_type type;
	struct packet *packet;
	int in_port_index;
	//int file_upload_dst;
	struct dns_job *next;
	// char localRootID;
	// int localRootDist;
	// char localParent;
	// char localPortTree[k];
};


struct dns_local_tree{
	char localRootID;
	int localRootDist;
	char localParent;
	//char localPortTree[k];
};


struct dns_job_queue {
	struct dns_job *head;
	struct dns_job *tail;
	int occ;
};
//
// struct thread_arg {
// 	struct dns_local_tree * tree_struct;
// 	int * inTree;
// 	int num_ports;
// 	int dns_id;
// 	struct dns_job_queue * job_q;
// 	struct dns_job_queue * tree_q;
// 	int * finalize;
// };

struct dns_routing_table_entry{
	char key;
	int port_num;
};

struct dns_entry{
	char key;
	char name[20];
};

void dns_main(int dns_id);
