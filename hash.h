#ifndef hash_h
#define hash_h

typedef void (*hash_data_free_func_t)(void *data);
typedef unsigned int (*hash_key_func_t)(const void *key, int klen);
typedef int (*hash_walk_func_t)(const void *key, int klen, void *val, void *data);

struct hash_node_st {
	void *key;
	int klen;					/* key and len */
	
	unsigned int __hval;		/* hash value of this node, for rehashing */
	void *val;					/* value */
	
	struct hash_node_st *next;
};

typedef struct hash_st {
	struct hash_node_st **slots;
	unsigned int nslot;			/* number of bucket */
	unsigned int max_element;	/* max element allowed for next rehashing */
	
	unsigned int nelement;		/* hash entry count */

	hash_data_free_func_t hdel;
	hash_key_func_t hkey;
} hash_st;

hash_st *xhash_create(hash_data_free_func_t del, hash_key_func_t keyf);
int xhash_walk(hash_st *ht, void *udata, hash_walk_func_t fn);
void xhash_destroy(hash_st *ht);
int xhash_delete(hash_st *ht, const void *key, int len);
int xhash_search(hash_st *ht, const void *key, int len, void **val);
void xhash_insert(hash_st *ht, const void *key, int len, void *val);
void hash_free_mem(void *data);


#endif