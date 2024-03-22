#include "sphere.h"
#include <stdlib.h>

sphereList *initSpheres() {
	sphereList *newList = (sphereList *)malloc(sizeof(sphereList));
	checkalloc(newList);
	newList->data = NULL;
	newList->next = NULL;
	return newList;
}


void addSphere(sphereList *list, vec3 center, rgb color, uint32_t radius, uint32_t spec, double reflectivity) {
	if (list == NULL) {
		fprintf(stderr, "You forgot to init the list.\n");
		return;
	}
	if (list->data == NULL) {
		list->data = (sphere *)malloc(sizeof(sphere));
		checkalloc(list->data);
		list->data->center = center;
		list->data->color = color;
		list->data->radius = radius;
		list->data->rSquare = radius * radius;
		list->data->specular = spec;
		list->data->reflectivity = reflectivity;
	} else {
		sphereList *curr = list;
		while (curr->next != NULL) {
			curr = curr->next;
		}
		curr->next = (sphereList *)malloc(sizeof(sphereList));
		checkalloc(curr->next);
		curr->next->data = (sphere *)malloc(sizeof(sphere));
		checkalloc(curr->next->data);
		curr->next->next = NULL;
		curr->next->data->center = center;
		curr->next->data->color = color;
		curr->next->data->radius = radius;
		curr->next->data->rSquare = radius * radius;
		curr->next->data->specular = spec;
		curr->next->data->reflectivity = reflectivity;
	}
}

void freeSphereList(sphereList *list) {
	sphereList *curr = list;
	while (curr != NULL) {
		if (curr->data != NULL) {
			free(curr->data);
		}
		sphereList *temp = curr->next;
		free(curr);
		curr = temp;
	}
}