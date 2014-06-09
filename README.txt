Original code from Linux Device Driver 3rd Edition (book).

Several minor modifications were made to the book's original source code. The
majority were taken from the following github repository:
    https://github.com/duxing2007/ldd3-examples-3.x

Code further modified by Preston Hamlin to add a new device called scullsort,
    which sorts the input characers such that the lowest valued characters in
    the buffer are read first. This modification consists of the new sort.c
    file, minor motifications to the other code files and additional entries in
    the makefile and load/unload scripts.


TODO: Have scull_sort_write peroem incremental writes rather than block until
        the entire text will fit.



                        === scullsort documentation ===
The scullsort device driver is derived from the scullpipe device driver. The
    diferences of note are the renaming of otherwise idential functions and the
    changes in background tasks so as to acomplish the soring functionality.

scull_sort_open - called to open the device file
    Increments the number of readers and writers as per flags from the provided
    file pointer. Effectively grants a "session" with the device.
    
scull_sort_release - called to release (close) the device file
    Decrements the number of readers and writers as per flags from the provided
    file pointer.

compare_helper - comparison funcion passed to sort function
    Compares two pointers casted to chars.

spacefree - calculates the amount of immediately usable space in the buffer
    By some simple pointer arithmetic, the amount of memory readily usable in
    the buffer is calculated.

scull_sort_read - reads and removes elements from buffer
    Since the buffer is a linear array, a read pointer is simply incremented
    beyond the read characters. The characters traced out in each read are
    copied to a userspace buffer. When a significant portion of the array is
    "hidden" behind the read pointer, the buffer is shifted to free up this
    space via scull_shift_buffer.
    This function blocks on an empty buffer and will unblock to read either the
    amount requested or the entire contents of the buffer, whichever is lesser.
    To minimize the amount of wasted sort operations, the buffer is sorted only
    when a read operation follows a write. That is, precisely that event which
    makes the buffer unsorted.

scull_sort_write - writes elements into the bufer
    Places elements at the position of the write pointer. If trying to write 
    more elements than there is room in the buffer, it writes what it can and
    waits for readers to free space in the buffer until all its contents are
    written.
    To provide as much room as possible, the buffer is cleansed of old data via
    scull_shift_buffer.

scull_sort_poll - polls the status of device

scull_sort_fasync - manages asynchronous readers

scull_sort_init - initializes device data
    
scull_sort_cleanup - frees device data

print_stuff - a function used for debugging device access patterns
    Prints device data to kernel log.

scull_shift_buffer - shifts buffer contents and updates pointers
    Since the buffer does not wrap around, the space trailing behind the read
    pointer needs to be reclamed. Each unread element is simply moved to a
    physical location in memory representative of its sorted respective
    position.

scull_sort_sortstuff - sorts buffer region between read and write pointers







                        === Improvements Upon SCULL ===
While the scull_getwritespace function is useful for modularizing the waiting
    a writer does, it was overly complicated considering that there is only one
    scullsort device. As such I incorporated it into the write function,
    restructured the blocking mechanism, and optimized the waiting somewhat by
    putting the reader to sleep for a lengthy period, since it will probably
    take multiple readers to free up enouth space and trigger a reorganizing of
    the buffer contents.


                          === Creative Features ===
Does nicely formatted debugging messages count?


                          === Reference Manual ===
You are reading it.





                                === TASKS ===
-Paper print-out of code and tests
-Mark changed/added code
-Email repository

-Tests ready, no editing required               -DON
-Read operation sorts written data correctly    -DONE
-Read on near-empty buffer takes it all         -DONE
-Read block IFF not O_NONBLOCK                  -DON
-Read removes elements from buffer              -DONE

-Write on full waits if not O_NONBLOCK          -DONE
    Write on full returns if O_NONBLOCK         -DON
-Write unblock on more space                    -DONE

-SCULL_IOCRESET empty buffer
-SORT allows concurrent access                  -DONE
-Persistant data                                -DONE
-Correct use of locking in critical sections    -DONE
-Preserved the old types of devices             -DONE
-Useless code                                   -DO
-Check the validity of all calls


Optional Features
    Test Programs           +5                  -DO
    Creative features       +10 
    Reference manual        +5                  -DO
    Source control          +5                  -DONE
    Correct/improve scull   +5                  -DO

