#include "light.h"
#include <stdlib.h>

light* initLights() {
	light *newLight = (light *)malloc(sizeof(light));
	checkalloc(newLight);
	newLight->ambient = 0.0;
	newLight->dirList = NULL;
	newLight->pointList = NULL;
	return newLight;
}

void setAmbient(light *light, double intensity) {
	light->ambient = intensity;
}

void addPLight(light *list, vec3 *pos, double intensity) {
	pointLightList *curr = list->pointList;
	if (curr == NULL) {
		list->pointList = (pointLightList *)malloc(sizeof(pointLightList));
		checkalloc(list->pointList);
		list->pointList->next = NULL;
		list->pointList->data = (pointLight *)malloc(sizeof(pointLight));
		checkalloc(list->pointList->data);
		list->pointList->data->pos = pos;
		list->pointList->data->intensity = intensity;
	} else {
		while (curr->next != NULL) {
			curr = curr->next;
		}
		curr->next = (pointLightList *)malloc(sizeof(pointLightList));
		checkalloc(curr->next);
		curr->next->next = NULL;
		curr->next->data = (pointLight *)malloc(sizeof(pointLight));
		checkalloc(curr->next->data);
		curr->next->data->pos = pos;
		curr->next->data->intensity = intensity;
	}
}

void addDLight(light *list, vec3 *dir, double intensity) {
	dirLightList *curr = list->dirList;
	if (curr == NULL) {
		list->dirList = (dirLightList *)malloc(sizeof(dirLightList));
		checkalloc(list->dirList);
		list->dirList->next = NULL;
		list->dirList->data = (dirLight *)malloc(sizeof(dirLight));
		checkalloc(list->dirList->data);
		list->dirList->data->dir = dir;
		list->dirList->data->intensity = intensity;
	} else {
		while (curr->next != NULL) {
			curr = curr->next;
		}
		curr->next = (dirLightList *)malloc(sizeof(dirLightList));
		checkalloc(curr->next);
		curr->next->next = NULL;
		curr->next->data = (dirLight *)malloc(sizeof(dirLight));
		checkalloc(curr->next->data);
		curr->next->data->dir = dir;
		curr->next->data->intensity = intensity;
	}
}

void freeLights(light *light) {
	dirLightList *currDir = light->dirList;
	while (currDir != NULL) {
		dirLightList *temp = currDir->next;
		free(currDir->data);
		free(currDir);
		currDir = temp;
	}

	pointLightList *currP = light->pointList;
	while (currP != NULL) {
		pointLightList *temp = currP->next;
		free(currP->data);
		free(currP);
		currP = temp;
	}

	free(light);
}