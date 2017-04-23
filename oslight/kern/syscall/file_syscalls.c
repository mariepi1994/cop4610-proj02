/*xxxx
 * File-related system call implementations.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>

/*
 * open() - get the path with copyinstr, then use openfile_open and
 * filetable_place to do the real work.
 */
int
sys_open(const_userptr_t upath, int flags, mode_t mode, int *retval)
{
	//const int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;

	char *kpath = (char*)kmalloc(sizeof(char)*PATH_MAX);
	struct openfile *file;
	int result = 0;

    	result = copyinstr(upath, kpath, PATH_MAX, NULL);
	if(result != 0) return result;
    	result = openfile_open(kpath,flags, mode, &file);
	if(result != 0) return result;
  	result = filetable_place(curproc->p_filetable, file, retval);
	if(result != 0) return result;

	return result;
}

/*
 * read() - read data from a file
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
       int result = 0;

       retval = 0;
       struct iovec vec;
       struct uio ou;
       struct filetable * ft = curproc->p_filetable; //correct?
       //struct openfile* ofile = curproc->p_filetable[fd]; //get the open file
       struct openfile* opened_file;

        //1. translate the file descriptor number to an open file object(use filetable_get)
        result = filetable_get(ft, fd, &opened_file);
	if(result != 0) return result;

       //2. lock the seek position in the open file (but only for seekable objects)
       lock_acquire(opened_file->of_offsetlock); //lock the file

       //3. check for files opened write-only
        ///check flag???
        if(opened_file->of_accmode == O_WRONLY){
            lock_release(opened_file->of_offsetlock);
            result = EACCES; //permission denied code
            return result;
        }

       //4. cons up a uio
       uio_kinit(&vec, &ou, buf, size, opened_file->of_offset,UIO_READ);

       //5. call VOP_READ
       result = VOP_READ(opened_file->of_vnode, &ou);
	if(result != 0) return result;

       //6. update the seek position afterwards
       opened_file->of_offset = ou.uio_offset;

       //7.unlock and filetable_put()
       lock_release(opened_file->of_offsetlock);
       filetable_put(ft,fd,opened_file);

       //8.set the return value correctly
	result = 0;


       (void) fd; // suppress compilation warning until code gets written
       (void) buf; // suppress compilation warning until code gets written
       (void) size; // suppress compilation warning until code gets written
       (void) retval; // suppress compilation warning until code gets written

       return result;
}

/*
 * write() - write data to a file
 */
int sys_write(int fd, userptr_t buf, size_t size, int *retval)
{
    	int result = 0;
    	retval = 0;
    	struct iovec vec;
    	struct uio ou;
    	struct filetable * ft = curproc->p_filetable; //correct?
    	//struct openfile* ofile = curproc->p_filetable[fd]; //get the open file
    	struct openfile* opened_file;

     	//1. translate the file descriptor number to an open file object(use filetable_get)
     	result = filetable_get(ft, fd, &opened_file);
	if(result != 0) return result;

    	//2. lock the seek position in the open file (but only for seekable objects)
    	lock_acquire(opened_file->of_offsetlock); //lock the file

    	//3. check for files opened read-only
     	///check flag???
     	if(opened_file->of_accmode == O_RDONLY){
         	lock_release(opened_file->of_offsetlock);
         	result = ENOENT; //permission denied code
     		return result;
     	}

    	//4. cons up a uio
    	uio_kinit(&vec, &ou, buf, size, opened_file->of_offset,UIO_WRITE);

    	//5. call VOP_READ
    	result = VOP_WRITE(opened_file->of_vnode, &ou);
	if(result != 0) return result;

   	//6. update the seek position afterwards
   	opened_file->of_offset = ou.uio_offset;

    	//7.unlock and filetable_put()
    	lock_release(opened_file->of_offsetlock);
    	filetable_put(ft,fd,opened_file);

    	//8.set the return value correctly
    	(void) retval;


	return result;
}

/*
 * close() - remove from the file table.
 */
int sys_close(int fd)
{
    struct filetable * ft = curproc->p_filetable;
    struct openfile* opened_file;


    //1. validate the fd number (use filetable_okfd)
    KASSERT(filetable_okfd(ft, fd));
    //2. use filetable_placeat to replace curproc's file table entry with NULL
    filetable_placeat(ft, NULL, fd, &opened_file);
    //3. check if the previous entry in the file table was also NULL(this means
    //no such file was open)
    if(opened_file == NULL){
        return ENOENT;
    }
    //4. decref the open file returned by filetable_placeat
    openfile_decref(opened_file);

    return 0;

}
/*
* encrypt() - read and encrypt the data of a file
*/
int sys_encrypt(const_userptr_t upath, size_t filesize)
{
	//Initializations
	int result = 0;
	char * kpath = (char*)kmalloc(sizeof(char)*PATH_MAX);
	struct openfile *file;
	int fd;
	struct iovec vec;
	struct uio ou;
	uint32_t buf = 0;
	int retval;

	//Get the last four encrypted and unencrypted four byte values
	//Encrypted set
	uint32_t printVal1 = 0;
	uint32_t printVal2 = 0;
	uint32_t printVal3 = 0;
	uint32_t printVal4 = 0;
	//Unencrypted Set
	uint32_t preVal1 = 0;
	uint32_t preVal2 = 0;
	uint32_t preVal3 = 0;
	uint32_t preVal4 = 0;
	//Opening of a file and placing it in file table
	//We do not close the file

	result = copyinstr(upath, kpath, PATH_MAX, NULL);
	if(result != 0) return result;
	result = openfile_open(kpath, O_RDWR, 0664, &file);
	if(result != 0) return result;
	result = filetable_place(curproc->p_filetable, file, &retval);
	if(result != 0) return result;

	fd = retval;

	lock_acquire(file->of_offsetlock);
	size_t i = 0;
	while(i < filesize)
	{
		if(i + 4 > filesize)
		{
			buf = 0;
			//Read four bytes of data from the file
			uio_kinit(&vec, &ou, &buf, filesize - i/*size*/, file->of_offset, UIO_READ);
			result = VOP_READ(file->of_vnode, &ou);
			if(result != 0) return result;
			//file->of_offset = ou.uio_offset;
			buf = buf | (uint32_t)0;
		}
		else
		{
			uio_kinit(&vec, &ou, &buf, 4/*size*/, file->of_offset, UIO_READ);
			result = VOP_READ(file->of_vnode, &ou);
				if(result != 0) return result;
		}
		//Apply cast as an unsigned 32bit integer
		uint32_t val = buf;

		//Save the preencrypted values
		preVal4 = preVal3;
		preVal3 = preVal2;
		preVal2 = preVal1;
		preVal1 = buf;

		//Cyclical right shift of 32bit value
		buf = (val >> 10) | (val << 22);

		//Save the encrypted values for printing
		printVal4 = printVal3;
		printVal3 = printVal2;
		printVal2 = printVal1;
		printVal1 = buf;

		uio_kinit(&vec, &ou, &buf, 4/*size*/, file->of_offset, UIO_WRITE);
	    	//5. call VOP_WRITE
	    	result = VOP_WRITE(file->of_vnode, &ou);

	   	if(result != 0) return result;
		//6. update the seek position afterwards
	    	file->of_offset = ou.uio_offset;
		i += 4;
	}
	lock_release(file->of_offsetlock);
    	filetable_put(curproc->p_filetable, fd, file);

	KASSERT(filetable_okfd(curproc->p_filetable, fd));
	//2. use filetable_placeat to replace curproc's file table entry with NULL
	filetable_placeat(curproc->p_filetable, NULL, fd, &file);
	//3. check if the previous entry in the file table was also NULL(this means
	//no such file was open)
	if(file == NULL) return ENOENT;
	//4. decref the open file returned by filetable_placeat
	openfile_decref(file);

	kprintf("Last Four Bytes\n");
	kprintf("Byte Encrypted\tUnencrypted\n");
	kprintf("00:  %x \t%x\n", printVal4, preVal4);
	kprintf("04:  %x \t%x\n", printVal3, preVal3);
	kprintf("08:  %x \t%x\n", printVal2, preVal2);
	kprintf("12:  %x \t%x\n", printVal1, preVal1);

	return 0;
}
