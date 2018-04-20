/*
 * switch.h
 */


struct switch_job {
	struct packet *packet;
	int in_port_index;
	//int file_upload_dst;
	struct switch_job *next;
	char localRootID;
	int localRootDist;
	char localParent;
	char localPortTree[k];
};

/*
struct switch_local_tree{
	char localRootID;
	int localRootDist;
	char localParent;
	char localPortTree[k];
}
*/

struct switch_job_queue {
	struct switch_job *head;
	struct switch_job *tail;
	int occ;
};

struct routing_table_entry{
	char key;
	int port_num;
};

void switch_main(int host_id);
