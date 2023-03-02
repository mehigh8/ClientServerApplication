#ifndef LIST_H_
#define LIST_H_

struct node {
	void *data;
	struct node* next;
};

struct list {
	struct node* head;
	int size;
	unsigned int data_size;
};

struct list* list_create(unsigned int data_size);

void list_add_nth_node(struct list* list, int n, const void* data);

struct node* list_remove_nth_node(struct list* list, int n);

struct node* list_get_nth_node(struct list* list, int n);

void list_free(struct list** pp_list);

#endif  /* LIST_H_ */