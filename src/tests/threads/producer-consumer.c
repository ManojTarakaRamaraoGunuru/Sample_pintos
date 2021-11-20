/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define buffer_size 4

char buffer[buffer_size];    /*shared buffer*/
int add=0;                  /*place to add next element in the buffer*/
int rem=0;                  /*place to remove next element in the buffer*/
int num=0;                  /*number of elements in buffer */

int pro_v=0, cons_v=0;

struct lock m;
struct condition c_cons;
struct condition c_prod;

char inp[11]="Hello World";

void producer_consumer(unsigned int num_producer, unsigned int num_consumer);


void test_producer_consumer(void)
{
    /*producer_consumer(0, 0);
    producer_consumer(1, 0);
    producer_consumer(0, 1);
    producer_consumer(1, 1);
    producer_consumer(3, 1);
    producer_consumer(1, 3);
    producer_consumer(4, 4);
    producer_consumer(7, 2);
    producer_consumer(2, 7);*/
    producer_consumer(6, 6);
    pass();
}

void*producer(void*arg){
    while(pro_v<11){
        lock_acquire(&m);
        // if(num>buffer_size){
        //     exit(1); // to avoid overflows
        // }
        while(num==buffer_size){
            cond_wait(&c_prod,&m);
        }
        buffer[add]=inp[pro_v];
        add=(add+1)%buffer_size;
        num++;
        pro_v++;
        cond_signal(&c_cons,&m);
        lock_release(&m);
    }
}

void*consumer(void*arg){

    while(cons_v<11){
        lock_acquire(&m);
        // if(num>buffer_size){
        //     exit(1); // to avoid overflows
        // }
        while(num==0){
            cond_wait(&c_cons,&m);
        }
        if(cons_v<11)printf("%c",buffer[rem]);
        rem=(rem+1)%buffer_size;
        num--;
        cons_v++;
        cond_signal(&c_prod,&m);
        lock_release(&m);
        
    }
}

void producer_consumer(UNUSED unsigned int num_producer, UNUSED unsigned int num_consumer)
{
    /* FIXME implement */
    lock_init(&m);
    cond_init(&c_prod);
    cond_init(&c_cons);
    for(int i=0;i<num_producer;i++){
        thread_create("prod",PRI_DEFAULT,producer,NULL);
    }
    for(int j=0;j<num_consumer;j++){
        thread_create("cons",PRI_DEFAULT,consumer,NULL);
    }

}

