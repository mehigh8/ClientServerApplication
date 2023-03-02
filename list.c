#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

struct list* list_create(unsigned int data_size)
{
	struct list* list = calloc(1, sizeof(struct list));
	if (list == NULL) {
		fprintf(stderr, "list calloc\n");
		exit(0);
	}

	list->data_size = data_size;
	return list;
}

static struct node* create_node(const void* new_data, unsigned int data_size)
{
    struct node* node = calloc(1, sizeof(struct node));
    if (node == NULL) {
    	fprintf(stderr, "node calloc\n");
    	exit(0);
    }

    node->data = malloc(data_size);
    if (node->data == NULL) {
    	fprintf(stderr, "node data malloc\n");
    	exit(0);
    }

    memcpy(node->data, new_data, data_size);

    return node;
}

void list_add_nth_node(struct list* list, int n, const void* new_data)
{
    struct node *new_node, *prev_node;

    if (list == NULL)
        return;

    new_node = create_node(new_data, list->data_size);

    if (n == 0 || list->size == 0) {
        new_node->next = list->head;
        list->head = new_node;
    } else {
        prev_node = list_get_nth_node(list, n - 1);
        new_node->next = prev_node->next;
        prev_node->next = new_node;
    }

    list->size++;
}

struct node* list_remove_nth_node(struct list* list, int n)
{
    struct node *prev_node, *removed_node;

    if (list == NULL || list->size == 0)
        return NULL;

    if (n == 0) {
        removed_node = list->head;
        list->head = removed_node->next;
        removed_node->next = NULL;
    } else {
        prev_node = list_get_nth_node(list, n - 1);
        removed_node = prev_node->next;
        prev_node->next = removed_node->next;
        removed_node->next = NULL;
    }

    list->size--;

    return removed_node;
}

struct node* list_get_nth_node(struct list* list, int n)
{
    struct node* node = list->head;

    if (n > list->size - 1)
    	n = list->size - 1;

    for (int i = 0; i < n; ++i)
        node = node->next;

    return node;
}

void list_free(struct list** pp_list)
{
    struct node* node;

    if (pp_list == NULL || *pp_list == NULL)
        return;

    while ((*pp_list)->size) {
        node = list_remove_nth_node(*pp_list, 0);
        free(node->data);
        free(node);
    }

    free (*pp_list);
    *pp_list = NULL;
}