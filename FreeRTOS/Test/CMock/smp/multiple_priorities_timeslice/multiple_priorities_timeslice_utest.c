/*
 * FreeRTOS V202012.00
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */
/*! @file multiple_priorities_timeslice_utest.c */

/* C runtime includes. */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Tasl includes */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "event_groups.h"
#include "queue.h"

/* Test includes. */
#include "unity.h"
#include "unity_memory.h"
#include "../global_vars.h"
#include "../smp_utest_common.h"

/* Mock includes. */
#include "mock_timers.h"
#include "mock_fake_assert.h"
#include "mock_fake_port.h"

/* ===========================  EXTERN VARIABLES  =========================== */

extern volatile UBaseType_t uxDeletedTasksWaitingCleanUp;

/* ============================  Unity Fixtures  ============================ */
/*! called before each testcase */
void setUp( void )
{
    commonSetUp();
}

/*! called after each testcase */
void tearDown( void )
{
    commonTearDown();
}

/*! called at the beginning of the whole suite */
void suiteSetUp()
{
}

/*! called at the end of the whole suite */
int suiteTearDown( int numFailures )
{
    return numFailures;
}

/* ==============================  Test Cases  ============================== */

/**
 * @brief AWS_IoT-FreeRTOS_SMP_TC-XX
 * A task of equal priority will be created for each available CPU core. An
 * additional task will be created in the ready state. This test will verify
 * that as OS ticks are generated the ready task will be made to run on each 
 * CPU core.
 * 
 * #define configRUN_MULTIPLE_PRIORITIES                    1
 * #define configUSE_TIME_SLICING                           1
 * #define configUSE_CORE_AFFINITY                          1
 * #define configNUM_CORES                                  (N > 1)
 * 
 * This test can be run with FreeRTOS configured for any number of cores greater than 1 .
 * 
 * Tasks are created prior to starting the scheduler.
 * 
 * Task (TN)	    Task (TN + 1)
 * Priority – 1     Priority – 1
 * State - Ready	State - Ready
 * 
 * After calling vTaskStartScheduler()
 * 
 * Task (TN)	               Task (TN + 1)
 * Priority – 1                Priority – 1
 * State - Running (Core N)	   State - Ready
 * 
 * Call xTaskIncrementTick() for each configured CPU core. The kernel will consider CPU 0
 * the core calling the API and therefore will not rotate tasks for that CPU.
 * 
 * Task (TN + 1) when configNUM_CORES = 4
 * Tick    Core
 * 1       1
 * 2       2
 * 3       3
 * 4      -1 
 */
void test_timeslice_verification_tasks_equal_priority( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 2] = { NULL };
    uint32_t i;

    /* Create configNUM_CORES + 1 tasks of high priority */
    for (i = 0; i < (configNUM_CORES + 1); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[i] );
    }
    
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );

    vTaskStartScheduler();

    /* Verify all configNUM_CORES tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* Verify the last task is in the ready state */
    verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );
    verifySmpTask( &xTaskHandles[configNUM_CORES+1], eReady, -1 );

    /* Generate a tick for each configNUM_CORES. This will cause each
       task to be either moved to the ready state or the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        
        xTaskIncrementTick();

        int32_t core = i + 1;

        /* Track wrap around to the ready state */
        if (core == configNUM_CORES) {
            core = -1;
        }

        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );
        verifySmpTask( &xTaskHandles[configNUM_CORES+1], eReady, -1 );
    }
}

void test_timeslice_verification_2( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;

    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[0] );

    /* Create configNUM_CORES - 1 tasks of low priority */
    for (i = 1; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );
    }
    
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );

    vTaskStartScheduler();

    /* Verify all configNUM_CORES tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* Verify the last task is in the ready state */
    verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );

    /* Generate a tick for each configNUM_CORES. This will cause each
       task to be either moved to the ready state or the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        
        xTaskIncrementTick();

        int32_t core = i + 1;

        /* Track wrap around to the ready state */
        if (core == configNUM_CORES) {
            core = -1;
        }

        /* Task T0 will always be running on core 0 */
        verifySmpTask( &xTaskHandles[0], eRunning, 0 );

        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );
    }
}

void test_timeslice_verification_3( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;

    /* Create configNUM_CORES tasks of high priority */
    for (i = 0; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[i] );
    }

    /* Create a single low priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );

    vTaskStartScheduler();

    /* Verify all tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* The remaining task shall be in the ready state */
    verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );

    /* Verify all tasks remain in the running state each time a tick is incremented */
    /* The low priority task should never enter the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        xTaskIncrementTick();
        
        for (int j = 0; j < configNUM_CORES; j++) {
            verifySmpTask( &xTaskHandles[j], eRunning, j );
        }

        verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );
    }

    /* Raise the priority of the low priority task to match the running tasks */
    vTaskPrioritySet( xTaskHandles[configNUM_CORES], 2 );

    /* After the first tick the ready task will be running on the last CPU core */
    int32_t core = (configNUM_CORES - 1);

    for (i = 0; i < configNUM_CORES; i++) {
        
        xTaskIncrementTick();

        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );

        /* Track wrap around to the ready state */
        if ((i % configNUM_CORES) == 0) {
            core = -1;
        } else {
            core = i % configNUM_CORES;
        }

        if (core == 0) {
            core = -1;
        }
    }
}

void test_timeslice_verification_4( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;

    /* Create configNUM_CORES tasks of equal priority */
    for (i = 0; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[i] );
    }
    
    /* Create a single equal priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[i] );

    vTaskStartScheduler();
    
    /* Verify all tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* The remaining task shall be in the ready state */
    verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );

    /* After the first tick the ready task will be running on CPU core 1 */
    int32_t core = 1;

    for (i = 0; i < configNUM_CORES; i++) {
        
        xTaskIncrementTick();

        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );

        /* Track wrap around to the ready state */
        if ((i % configNUM_CORES) == (configNUM_CORES - 2)) {
            core = -1;
        } else {
            core = (i % configNUM_CORES) + 2;
        }

        if ((i % configNUM_CORES) == (configNUM_CORES - 1)) {
            core = 1;
        }
    }

    /* Lower the priority of task T0 */
    vTaskPrioritySet( xTaskHandles[configNUM_CORES], 1 );

    /* Verify all configNUM_CORES tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        xTaskIncrementTick();
        
        for (int j = 0; j < configNUM_CORES; j++) {
            verifySmpTask( &xTaskHandles[j], eRunning, j );
        }

        /* Verify the low priority task remains in the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );
    }
}

void test_timeslice_verification_5( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;
    int32_t core;

    /* Create a single equal priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[0] );

    /* Create configNUM_CORES tasks of equal priority */
    for (i = 1; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );
    }
    
    /* Create a single equal priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[configNUM_CORES] );

    vTaskStartScheduler();
    
    /* Verify all tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* The remaining task shall be in the ready state */
    verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );

    for (i = 0; i < configNUM_CORES; i++) {
        
        xTaskIncrementTick();

        core = i + 1;

        /* Track wrap around to the ready state */
        if (core == configNUM_CORES) {
            core = -1;
        }

        /* Task T0 will always be running on core 0 */
        verifySmpTask( &xTaskHandles[0], eRunning, 0 );

        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );
    }

    /* Lower the priority of task T0 */
    vTaskPrioritySet( xTaskHandles[0], 1 );

    core = -1;

    for (i = 0; i < configNUM_CORES; i++) {

        xTaskIncrementTick();
       
        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[0], (core == -1) ? eReady : eRunning, core );

        /* Track wrap around to the ready state */
        if ((i % configNUM_CORES) == (configNUM_CORES - 1)) {
            core = -1;
        } else {
            core = (i % configNUM_CORES) + 1;
        }
    }
}

void test_timeslice_verification_6( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;
    int32_t core;

    /* Create a single equal priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[0] );

    /* Create configNUM_CORES tasks of equal priority */
    for (i = 1; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );
    }
    
    /* Create a single equal priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[configNUM_CORES] );

    vTaskStartScheduler();
    
    /* Verify all tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* The remaining task shall be in the ready state */
    verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );

    for (i = 0; i < configNUM_CORES; i++) {
        
        xTaskIncrementTick();

        core = i + 1;

        /* Track wrap around to the ready state */
        if (core == configNUM_CORES) {
            core = -1;
        }

        /* Task T0 will always be running on core 0 */
        verifySmpTask( &xTaskHandles[0], eRunning, 0 );

        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );
    }

    /* Lower the priority of task T0 */
    vTaskPrioritySet( xTaskHandles[configNUM_CORES], 2 );

    core = -1;

    for (i = 0; i < configNUM_CORES; i++) {

        xTaskIncrementTick();

        /* Tasks will always be running on core 0 and the last core */
        verifySmpTask( &xTaskHandles[0], eRunning, 0 );
        verifySmpTask( &xTaskHandles[configNUM_CORES], eRunning, (configNUM_CORES -1) );

        if ( configNUM_CORES > 2) {
            
            /* Verify the last created task runs on each core or enters the ready state */
            verifySmpTask( &xTaskHandles[(configNUM_CORES - 1)], (core == -1) ? eReady : eRunning, core );

            /* Track wrap around to the ready state */
            if ((i % configNUM_CORES) == (configNUM_CORES - 2)) {
                core = -1;
            } else {
                core = (i % configNUM_CORES) + 1;
            }
        }
    }
}

void test_timeslice_verification_7( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;

    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[0] );

    /* Create configNUM_CORES + 1 tasks of high priority */
    for (i = 1; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );
    }

    vTaskStartScheduler();

    /* Verify all configNUM_CORES tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* Verify all tasks remain in the running state each time a tick is incremented */
    for (i = 0; i < configNUM_CORES; i++) {
        xTaskIncrementTick();
        
        for (int j = 0; j < configNUM_CORES; j++) {
            verifySmpTask( &xTaskHandles[j], eRunning, j );
        }
    } 

    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[configNUM_CORES] );

    /* Generate a tick for each configNUM_CORES. This will cause each
    task to be either moved to the ready state or the running state */
    int32_t core = -1;

    for (i = 0; i < configNUM_CORES; i++) {

        xTaskIncrementTick();
       
        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );

        verifySmpTask( &xTaskHandles[0], eRunning, 0 );

        /* Track wrap around to the ready state */
        if ((i % configNUM_CORES) == (configNUM_CORES - 1)) {
            core = -1;
        } else {
            core = (i % configNUM_CORES) + 1;
        }
    }
}

void test_timeslice_verification_8( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;

    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[0] );

    /* Create configNUM_CORES + 1 tasks of high priority */
    for (i = 1; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );
    }

    vTaskStartScheduler();

    /* Verify all configNUM_CORES tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* Verify all tasks remain in the running state each time a tick is incremented */
    for (i = 0; i < configNUM_CORES; i++) {
        xTaskIncrementTick();
        
        for (int j = 0; j < configNUM_CORES; j++) {
            verifySmpTask( &xTaskHandles[j], eRunning, j );
        }
    } 

    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[configNUM_CORES] );

    /* Generate a tick for each configNUM_CORES. This will cause each
    task to be either moved to the ready state or the running state */
    int32_t core = -1;

    for (i = 0; i < configNUM_CORES; i++) {

        xTaskIncrementTick();
       
        /* Verify the last created task runs on each core or enters the ready state */
        //verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );
        verifySmpTask( &xTaskHandles[configNUM_CORES], eRunning, (configNUM_CORES - 1) );
        verifySmpTask( &xTaskHandles[0], eRunning, 0 );

        if ( configNUM_CORES > 2) {
            
            /* Verify the last created task runs on each core or enters the ready state */
            verifySmpTask( &xTaskHandles[(configNUM_CORES - 1)], (core == -1) ? eReady : eRunning, core );

            /* Track wrap around to the ready state */
            if ((i % configNUM_CORES) == (configNUM_CORES - 2)) {
                core = -1;
            } else {
                core = (i % configNUM_CORES) + 1;
            }
        }
    }
}

void test_timeslice_verification_9( void )
{
    TaskHandle_t xTaskHandles[configNUM_CORES + 1] = { NULL };
    uint32_t i;
    int32_t core;

    /* Create a single equal priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 2, &xTaskHandles[0] );

    /* Create configNUM_CORES tasks of equal priority */
    for (i = 1; i < (configNUM_CORES); i++) {
        xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i] );
    }
    
    /* Create a single equal priority task */ 
    xTaskCreate( vSmpTestTask, "SMP Task", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[configNUM_CORES] );

    vTaskStartScheduler();
    
    /* Verify all tasks are in the running state */
    for (i = 0; i < configNUM_CORES; i++) {
        verifySmpTask( &xTaskHandles[i], eRunning, i );
    }

    /* The remaining task shall be in the ready state */
    verifySmpTask( &xTaskHandles[configNUM_CORES], eReady, -1 );

    for (i = 0; i < configNUM_CORES; i++) {
        
        xTaskIncrementTick();

        core = i + 1;

        /* Track wrap around to the ready state */
        if (core == configNUM_CORES) {
            core = -1;
        }

        /* Task T0 will always be running on core 0 */
        verifySmpTask( &xTaskHandles[0], eRunning, 0 );

        /* Verify the last created task runs on each core or enters the ready state */
        verifySmpTask( &xTaskHandles[configNUM_CORES], (core == -1) ? eReady : eRunning, core );
    }

    /* Delete last task */
    vTaskDelete(xTaskHandles[configNUM_CORES]);
}
