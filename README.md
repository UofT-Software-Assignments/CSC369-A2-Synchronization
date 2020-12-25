# CSC369-A2-Synchronization

A solution to a multiple produce multiple consumer problem on a ring buffer

## Disclaimer

This repository is not solely my own code, this was an assignment completed for the University of Toronto's course CSC369 - Operating Systems, 
and is formed by my contributions to the starter code of the assignment

My contributions are as follows:

in msg_queue.c, I implemented all functions with the exception of 

- mq_close
- mq_open
- make_handle
- get_backend
- mq_destroy
- mq_init 

As well as all structs with the exception of mq_backend where my contributions were the following struct attributes:
 - queue_mutex
 - empty
 - no_space
