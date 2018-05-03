/*
 * switch.h
 */

enum switch_job_type {
	JOB_FORWARD_PACKET,
	JOB_TREE_SEND,
	JOB_TREE_WAITING,
};

struct switch_job {
	enum switch_job_type type;
	struct packet *packet;
	int in_port_index;
	//int file_upload_dst;
	struct switch_job *next;
	// char localRootID;
	// int localRootDist;
	// char localParent;
	// char localPortTree[k];
};


struct switch_local_tree{
	char localRootID;
	int localRootDist;
	char localParent;
	//char localPortTree[k];
};


struct switch_job_queue {
	struct switch_job *head;
	struct switch_job *tail;
	int occ;
};

struct thread_arg {
	struct switch_local_tree * tree_struct;
	int * inTree;
	int num_ports;
	int switch_id;
	struct switch_job_queue * job_q;
	struct switch_job_queue * tree_q;
	int * finalize;
};

struct routing_table_entry{
	char key;
	int port_num;
};

void switch_main(int host_id);
