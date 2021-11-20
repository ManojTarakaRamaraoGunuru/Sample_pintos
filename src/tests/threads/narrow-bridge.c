/* Tests producer/consumer communication with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

//#include "devices/timer.c"


void narrow_bridge(unsigned int num_vehicles_left, unsigned int num_vehicles_right,
        unsigned int num_emergency_left, unsigned int num_emergency_right);

struct vechile{
    int direc;  // determines direction if 0 left and 1 right
    int prio;   // determines direction if 1 first and 0 last
};

struct semaphore cap;   //indiates number of cars on the bridge
struct semaphore lock;

int tot_emer;
int emer_left;
int norm_left;

void test_narrow_bridge(void)
{
    /*narrow_bridge(0, 0, 0, 0);
    narrow_bridge(1, 0, 0, 0);
    narrow_bridge(0, 0, 0, 1);
    narrow_bridge(0, 4, 0, 0);
    narrow_bridge(0, 0, 4, 0);
    narrow_bridge(3, 3, 3, 3);
    narrow_bridge(4, 3, 4 ,3);
    narrow_bridge(7, 23, 17, 1);
    narrow_bridge(40, 30, 0, 0);
    narrow_bridge(30, 40, 0, 0);
    narrow_bridge(23, 23, 1, 11);
    narrow_bridge(22, 22, 10, 10);
    narrow_bridge(0, 0, 11, 12);
    narrow_bridge(0, 10, 0, 10);*/
    narrow_bridge(0, 10, 10, 0);
    pass();
}

void exit_bridge(int direc, int prio){
    if(emer_left>0){
        //condition for removing emergency_left threads
        emer_left--;
    }
    if(tot_emer>0){
        //condition for removing emergency right threads(mainly) and emergency left threads
        tot_emer--;
    }
    if(prio==0 && direc==0)
    {
        //conditon for removing normal left threads.
        norm_left--;
    }
    //condition for removing any thread.
    sema_up(&cap);
}
void cross_bridge(int direc, int prio){
    
    if(prio==1){
        if(direc==0){
            printf("emergency vechile entered from right\n");
        }
        else{
            printf("emergency vechile entered from left\n");
        }
    }
    else{
        if(direc==0){
            printf("normal vechile entered from right\n");
        }
        else{
            printf("normal vechile entered from left\n");
        }
    }
    sema_down(&lock);
    int t=5;
    while(t--);
    sema_up(&lock);
    if(prio==1){
        if(direc==0){
            printf("emergency vechile exited from left\n");
        }
        else{
            printf("emergency vechile exited from right\n");
        }
    }
    else{
        if(direc==0){
            printf("normal vechile exited from left\n");
        }
        else{
            printf("normal vechile exited from right\n");
        }
    }
    
}
void arrive_bridge(int direc, int prio){
    if(prio==0){
        // to stop all threads which are not emergency....after emergency threads exit, normal threads will move.
        while(tot_emer!=0); 
    }
    // passing only emergency left threads first then emergency right threads next
    // if normal thread come here tht means emergency thread are finsihed(tot_emer=emer_left=0) so all noraml threads
    // will cross the below condition 
    while(emer_left!=0 && direc==1);
    if(prio==0 && direc==1)
    {
        //to stop all noraml left threads which are headed towards right....after all normal left threads exit, normal 
        // right threads will move.
        while(norm_left!=0);
    }
    sema_down(&cap);
}
void *one_vechile(struct vechile* args){
    struct vechile*v=args;
    arrive_bridge(v->direc, v->prio);
    cross_bridge(v->direc,v->prio);
    exit_bridge(v->direc, v->prio);
}

void narrow_bridge(UNUSED unsigned int num_vehicles_left, UNUSED unsigned int num_vehicles_right,
        UNUSED unsigned int num_emergency_left, UNUSED unsigned int num_emergency_right)
{
    /* FIXME implement */
    sema_init(&cap,3);
    sema_init(&lock,1);
    tot_emer=num_emergency_right+num_emergency_left;
    emer_left=num_emergency_left;
    norm_left=num_vehicles_left;

    //printf("Hello iiiiiiiii");
    for(int i=0;i<num_emergency_left;i++){
        struct vechile* x=(struct vechile*)malloc(sizeof(struct vechile));
        x->prio=1;
        x->direc=0;
        thread_create("left_emergency",0,one_vechile,x);
    }

    for(int i=0;i<num_emergency_right;i++){
        struct vechile* x=(struct vechile*)malloc(sizeof(struct vechile));
        x->prio=1;
        x->direc=1;
        thread_create("right_emergency",0,one_vechile,x);
    }
    for(int i=0;i<num_vehicles_left;i++){
        struct vechile* x=(struct vechile*)malloc(sizeof(struct vechile));
        x->prio=0;
        x->direc=0;
        thread_create("left_normal",0,one_vechile,x);
    }

    for(int i=0;i<num_vehicles_right;i++){
        struct vechile* x=(struct vechile*)malloc(sizeof(struct vechile));
        x->prio=0;
        x->direc=1;
        //printf("%d %d\n",x.prio,x.direc);
        thread_create("right_normal",0,one_vechile,x);
    }

    

}

