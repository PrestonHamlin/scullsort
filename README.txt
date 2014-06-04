Original code from Linux Device Driver 3rd Edition (book).

Several minor modifications were made to the book's original source code. The
majority were taken from the following github repository:
    https://github.com/duxing2007/ldd3-examples-3.x

Code further modified by Preston Hamlin to add a new device called scullsort,
    which sorts the input characers such that the lowest valued characters in
    the buffer are read first. This modification consists of the new sort.c
    file, minor motifications to the other code files and additional entries in
    the makefile and load/unload scripts.


                        === scullsort documentation ===
The scullsort device driver is derived from the scullpipe device driver. The
    diferences of note are the renaming of otherwise idential functions and the
    changes in background tasks so as to acomplish the soring functionality.



                                === TASKS ===
-Paper print-out of code and tests
-Mark changed/added code
-Email repository

-Tests ready, no editing required
-Read operation sorts written data correctly
-Read asking for more than in SORT returns everything left
-Read block IFF not O_NONBLOCK
-Read removes elements from buffer

-Write on full spins if not O_NONBLOCK
    Write on full returns immediately if O_NONBLOCK
-Write unblock on more space 

-SCULL_IOCRESET empty buffer
-SORT allows concurrent access
-Persistant data
-Correct use of locking in critical sections
-Preserved the old types of devices
-Useless code
-Check the validity of all calls


Optional Features
    Test Programs           +5
    Creative features       +10 
    Reference manual        +5
    Source control          +5
    Correct/improve scull   +5 

