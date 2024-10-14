#include "FreeRTOS.h"
#include "task.h"

#include "xtime_l.h"



void task1(void *arg);
void task2(void *arg);

int main(void){
	BaseType_t err = pdFREERTOS_ERRNO_NONE;

	xTaskCreate(task1, "task1", configMINIMAL_STACK_SIZE, ( void * ) NULL, tskIDLE_PRIORITY + 1, ( void * ) NULL);
	xTaskCreate(task2, "task2", configMINIMAL_STACK_SIZE, ( void * ) NULL, tskIDLE_PRIORITY + 1, ( void * ) NULL);



	if(err < 0){
		err = 0;
	}
	vTaskStartScheduler();
	return 0;
}



void task1(void *arg){

	int counter = 0;

	while(1){

		counter++;

		if(counter >= 10000){
			counter = 0;
		}

		vTaskDelay(pdMS_TO_TICKS(1));
	}


}


void task2(void *arg){

	int counter = 0;

	while(1){

		counter++;

		if(counter >= 10000){
			counter = 0;
		}

		vTaskDelay(pdMS_TO_TICKS(1));

	}


}
