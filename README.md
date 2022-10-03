# XV6-modified
Several modfication to XV-6 are done, each task represents a particular aspect of OS kernel, like memory management, scheduler, shell behaviour, etc. See each task detailed reports for better understanding

# Tasks and introductions
* Task-0 : Studying the boot order and memory flow in XV6 modules (Bootasm.S, Bootblock.asm) by debugging the code and changing the address values. Basics like how memory stack(RAM) is configured and partitions etc.

 ![image](https://user-images.githubusercontent.com/72460532/193620012-22aaaa00-3e91-4dd8-8285-67da25588d8f.png) ![image](https://user-images.githubusercontent.com/72460532/193620272-b0879090-f44c-4189-9c8b-1b5b4c652735.png)

* Task-1 : This is about how to add a Custom User program and system calls in XV6.

![image](https://user-images.githubusercontent.com/72460532/193620731-ba661a9d-70a3-42a9-b715-0e631d67edff.png)

* Task-2A : Here we added some curson operations in shell like left/right arrow function(which also includes editing) and shell history option which can be accessed using up and down arrow(Circular loop implementation, O(1) operation). Here we understand how shell handles consoles at different layers and buffers.

![image](https://user-images.githubusercontent.com/72460532/193621593-fe539ebf-f876-44f1-8e22-8e9a6ceead11.png)
![image](https://user-images.githubusercontent.com/72460532/193621795-d8b37fdd-6c0e-4a42-9f9d-9477ed518ac8.png)

* Task-2B : Here we customize XV6 scheduler with various policies - FCFS, Round Robin, DML, SML. We understand how the processes are activated, the process struct's parameters and process related system calls like myproc(), etc. And finally do a performance test between them.


![image](https://user-images.githubusercontent.com/72460532/193622255-12ef60fb-070e-469f-9d2e-52c8d83cb09c.png)
![image](https://user-images.githubusercontent.com/72460532/193622503-46fa6a49-523a-4b41-8d2f-b29a2488a26d.png) ![image](https://user-images.githubusercontent.com/72460532/193622779-321e2231-b962-41bd-ba0b-9b7f969cbd32.png)

* Task-3 : This is about memory management. How memory allocation is done, what are system calls involved, etc. Here we also implement Lazy memory allocation stategy by modifying memory allocater and pagefault_handler. And we will also Implement a swap-in/out mechanism at a kernal level which frees RAM by exploiting free disk space, it also takes requests from processes to bring back the pages/page-directories required by the processes.

![image](https://user-images.githubusercontent.com/72460532/193622960-50f073dc-d401-4fe9-abe7-6fc63023a1ee.png) ![image](https://user-images.githubusercontent.com/72460532/193622991-50b46078-054d-49c4-8ebb-4f2ce8f4e03f.png)
![image](https://user-images.githubusercontent.com/72460532/193623177-45f62303-d4c3-4355-ba20-73b433853372.png) ![image](https://user-images.githubusercontent.com/72460532/193623416-f153e06b-fe34-4de1-b922-b48c6be54501.png)

* Task-4 : This is a study on Linux process management and devising how to include some of the features into XV6. The features weren't implemented in XV6 code, the code flow required and the design are explained(System calls and logic). [Youtube presentation](https://youtu.be/SGtBPo8y9dA)
![image](https://user-images.githubusercontent.com/72460532/193623711-78a50aef-b48a-4c7c-a45b-6ba9e9d178ca.png)

***Note : All the screenshots are part of the reports, please go through the report for better understanding.***
#
This is done under Dr.John Jose, in Operating Systems course at IIT Guwahati.
