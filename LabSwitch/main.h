
#define BCAST_ADDR 100
#define PAYLOAD_MAX 100
#define STRING_MAX 100
#define NAME_LENGTH 100

enum NetNodeType { /* Types of network nodes */
	HOST,
	SWITCH,
	DNS
};

enum NetLinkType { /* Types of linkls */
	PIPE,
	SOCKET
};

struct net_node { /* Network node, e.g., host or switch */
	enum NetNodeType type;	//either Host or Switch or DNS
	int id;
	struct net_node *next;
};

struct net_port { /* port to communicate with another node */
	enum NetLinkType type;  //either Pipe or Socket
	int pipe_host_id;
	int pipe_send_fd;
	int pipe_recv_fd;
	struct net_port *next;
};

/* Packet sent between nodes  */

struct packet { /* struct for a packet */
	char src;		//tree: root_id
	char dst;		//tree: distance
	char type;	//tree: PKT_TREE
	int length;
	char payload[PAYLOAD_MAX];	//sender type H/S and child Y/N
};

/* Types of packets */

#define PKT_PING_REQ		0
#define PKT_PING_REPLY		1
#define PKT_FILE_UPLOAD_START	2
#define PKT_FILE_UPLOAD_END	3
#define PKT_FILE_DOWNLOAD_REQ   5
#define PKT_FILE_UPLOAD_IMD	6
#define PKT_TREE 7
#define DNS_REQ 8
#define DNS_REPLY 9
#define DNS_REGISTER 10
