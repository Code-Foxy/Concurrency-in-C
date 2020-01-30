#include "driver.h"

sem_t semSelect;

driver_t* driver_create(size_t size){

	driver_t* driver = (driver_t *) malloc(sizeof(driver_t));

	driver->queue = queue_create(size);
	driver->zero_flag = 0;
	driver->job_holder = NULL;

	if (size == 0){
		driver->zero_flag = 1;
	}

	driver->close_flag = 0;
	driver->thread_counter = 0;

	sem_init(&driver->semMaster, 0, 1);
	sem_init(&driver->semAllow, 0, (unsigned int) queue_capacity(driver->queue));
	sem_init(&driver->semBlock, 0, 0);
	sem_init(&driver->semUnqueueAllow, 0, 1);
	sem_init(&driver->semUnqueueBlock, 0, 0);
	sem_init(&driver->semUnqueuePairS, 0, 0);
	sem_init(&driver->semUnqueuePairH, 0, 0);
	sem_init(&driver->semProtect, 0, 1);

	return driver;
}


enum driver_status driver_schedule(driver_t *driver, void* job) {

	driver->thread_counter++;

	if(driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
		}
	/*--------------------BEGIN UNQUEUE--------------------*/
	if (driver->zero_flag == 1){

		sem_post(&driver->semUnqueuePairS);
		sem_wait(&driver->semUnqueueAllow);
		sem_wait(&driver->semMaster);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}

		driver->job_holder = job;
		sem_post(&driver->semUnqueueBlock);
		sem_post(&driver->semMaster);

		return SUCCESS;
	}
	/*---------------------END UNQUEUE---------------------*/
	sem_wait(&driver->semAllow);
	sem_wait(&driver->semMaster);

	if(driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
		}

	if (queue_add(driver->queue, job) == QUEUE_SUCCESS){
		sem_post(&driver->semMaster);
		sem_post(&driver->semBlock);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}
		return SUCCESS;
	}

	sem_post(&driver->semMaster);
	sem_post(&driver->semAllow);

	return DRIVER_GEN_ERROR;
}

enum driver_status driver_handle(driver_t *driver, void **job) {

	driver->thread_counter++;
	
	if (driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
	}
	/*--------------------BEGIN UNQUEUE--------------------*/
	if(driver->zero_flag == 1){

		sem_post(&driver->semUnqueuePairH);
		sem_wait(&driver->semUnqueueBlock);
		sem_wait(&driver->semMaster);

		if (driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}

		*job = driver->job_holder;

		sem_post(&driver->semProtect);
		sem_post(&driver->semUnqueueAllow);
		sem_post(&driver->semMaster);
		return SUCCESS;
	}
	/*---------------------END UNQUEUE---------------------*/
	sem_wait(&driver->semBlock);
	sem_wait(&driver->semMaster);

	if(driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
		}

	if(queue_remove(driver->queue, job) == QUEUE_SUCCESS){
		sem_post(&driver->semMaster);
		sem_post(&driver->semAllow);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}
		return SUCCESS;
	}

	sem_post(&driver->semMaster);
	sem_post(&driver->semBlock);
	return DRIVER_GEN_ERROR;
}

enum driver_status driver_non_blocking_schedule(driver_t *driver, void* job){

	driver->thread_counter++;

	if(driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
	}
	/*--------------------BEGIN UNQUEUE--------------------*/
	if(driver->zero_flag == 1){

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}
		
		sem_post(&driver->semUnqueuePairS);
		if(sem_trywait(&driver->semUnqueueAllow) == -1){
			return DRIVER_REQUEST_FULL;
		}

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}
		
		sem_wait(&driver->semProtect);
		sem_wait(&driver->semMaster);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}

		driver->job_holder = job;

		sem_post(&driver->semMaster);
		sem_post(&driver->semUnqueueBlock);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}
		
		if(sem_trywait(&driver->semUnqueuePairH) == -1){
			return DRIVER_REQUEST_FULL;
		}

		return SUCCESS;
	}
	/*---------------------END UNQUEUE---------------------*/

	if(sem_trywait(&driver->semAllow) == -1){
		return DRIVER_REQUEST_FULL;
	}

	if(driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
	}

	sem_wait(&driver->semMaster);
	if(queue_add(driver->queue, job) == QUEUE_SUCCESS){
		sem_post(&driver->semMaster);
		sem_post(&driver->semBlock);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}
		return SUCCESS;
	}

	sem_post(&driver->semMaster);
	sem_post(&driver-> semAllow);

	return DRIVER_GEN_ERROR;
}

enum driver_status driver_non_blocking_handle(driver_t *driver, void **job) {

	driver->thread_counter++;

	if(driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
	}
	/*--------------------BEGIN UNQUEUE--------------------*/
	if(driver->zero_flag == 1){

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}

		if(sem_trywait(&driver->semUnqueueBlock) == -1){
			return DRIVER_REQUEST_EMPTY;
		}

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}

		sem_wait(&driver->semMaster);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}

		*job = driver->job_holder;

		sem_post(&driver->semMaster);
		sem_post(&driver->semUnqueueAllow);

		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}

		if(sem_trywait(&driver->semUnqueuePairS) == -1){
			return DRIVER_REQUEST_EMPTY;
		}
		
		return SUCCESS;
	}
	/*---------------------END UNQUEUE---------------------*/

	if(sem_trywait(&driver->semBlock) == -1) {
		return DRIVER_REQUEST_EMPTY;
		}

	if(driver->close_flag == 1){
		return DRIVER_CLOSED_ERROR;
	}

	sem_wait(&driver->semMaster);
	if(queue_remove(driver->queue, job) == QUEUE_SUCCESS){
		sem_post(&driver->semMaster);
		sem_post(&driver->semAllow);
		if(driver->close_flag == 1){
			return DRIVER_CLOSED_ERROR;
		}
		return SUCCESS;
	}

	sem_post(&driver->semMaster);
	sem_post(&driver->semBlock);

	return DRIVER_GEN_ERROR;
}

enum driver_status driver_close(driver_t *driver) {

	if (driver == NULL){
		return DRIVER_GEN_ERROR;
	}

	if(driver->close_flag == 1){
		return DRIVER_GEN_ERROR;
	}
	
	driver->close_flag = 1;
	int i = 0;
	while (i < driver->thread_counter){
		sem_post(&driver->semMaster);
		sem_post(&driver->semAllow);
		sem_post(&driver->semBlock);
		sem_post(&driver->semUnqueueAllow);
		sem_post(&driver->semUnqueueBlock);
		sem_post(&driver->semUnqueuePairS);
		sem_post(&driver->semUnqueuePairH);
		i++;
	}
	return SUCCESS;
}

enum driver_status driver_destroy(driver_t *driver) {

	if (driver->close_flag != 1){
		return DRIVER_DESTROY_ERROR;	
	}
	
	sem_destroy(&driver->semMaster);
	sem_destroy(&driver->semAllow);
	sem_destroy(&driver->semBlock);
	sem_destroy(&driver->semUnqueueAllow);
	sem_destroy(&driver->semUnqueueBlock);
	sem_destroy(&driver->semUnqueuePairS);
	sem_destroy(&driver->semUnqueuePairH);
	sem_destroy(&driver->semProtect);
	sem_destroy(&semSelect);
	queue_free(driver->queue);
	free(driver);
	return SUCCESS;
}

enum driver_status driver_select(select_t *driver_list, size_t driver_count, size_t* selected_index) {

	sem_init(&semSelect, 0, 1);

	int semValue;
	semValue = 0;
	int i;
	int s;
	s = 1;
	sem_wait(&semSelect);
	while (s == 1){

		for(i = 0; i< driver_count; i++){

			if((driver_list + i)->driver->close_flag == 1){
				*selected_index = (unsigned int) i;
				sem_post(&semSelect);
				return DRIVER_CLOSED_ERROR;
			}

			if (driver_list->op == SCHDLE){
				sem_getvalue(&(driver_list + i)->driver->semAllow, &semValue);
				if (semValue > 0){
					*selected_index = (unsigned int) i;
					if (driver_non_blocking_schedule((driver_list + i)->driver, (driver_list)->job) == SUCCESS){
						sem_post(&semSelect);
						return SUCCESS;
					}
				}
			}
			else if(driver_list->op == HANDLE){
				sem_getvalue(&(driver_list + i)->driver->semBlock, &semValue);
				if (semValue > 0){
					*selected_index = (unsigned int) i;
					if (driver_non_blocking_handle((driver_list + i)->driver, &(driver_list)->job) == SUCCESS){
						sem_post(&semSelect);
						return SUCCESS;
					}
				}
			}
			if((driver_list + i)->driver == NULL){
				*selected_index = (unsigned int) i;
				sem_post(&semSelect);
				return DRIVER_GEN_ERROR;
			}
		}
	}
	return DRIVER_GEN_ERROR;
}
