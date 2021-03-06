/*
 * NVRAM variable manipulation (Linux kernel half)
 *
 * Copyright 2001-2003, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmendian.h>

#define NVRAM_SIZE       (0x1ff0)
static char _nvdata[NVRAM_SIZE] __initdata;
static char _valuestr[256] __initdata;

/*
 * TLV types.  These codes are used in the "type-length-value"
 * encoding of the items stored in the NVRAM device (flash or EEPROM)
 *
 * The layout of the flash/nvram is as follows:
 *
 * <type> <length> <data ...> <type> <length> <data ...> <type_end>
 *
 * The type code of "ENV_TLV_TYPE_END" marks the end of the list.
 * The "length" field marks the length of the data section, not
 * including the type and length fields.
 *
 * Environment variables are stored as follows:
 *
 * <type_env> <length> <flags> <name> = <value>
 *
 * If bit 0 (low bit) is set, the length is an 8-bit value.
 * If bit 0 (low bit) is clear, the length is a 16-bit value
 * 
 * Bit 7 set indicates "user" TLVs.  In this case, bit 0 still
 * indicates the size of the length field.  
 *
 * Flags are from the constants below:
 *
 */
#define ENV_LENGTH_16BITS	0x00	/* for low bit */
#define ENV_LENGTH_8BITS	0x01

#define ENV_TYPE_USER		0x80

#define ENV_CODE_SYS(n,l) (((n)<<1)|(l))
#define ENV_CODE_USER(n,l) ((((n)<<1)|(l)) | ENV_TYPE_USER)

/*
 * The actual TLV types we support
 */

#define ENV_TLV_TYPE_END	0x00	
#define ENV_TLV_TYPE_ENV	ENV_CODE_SYS(0,ENV_LENGTH_8BITS)

/*
 * Environment variable flags 
 */

#define ENV_FLG_NORMAL		0x00	/* normal read/write */
#define ENV_FLG_BUILTIN		0x01	/* builtin - not stored in flash */
#define ENV_FLG_READONLY	0x02	/* read-only - cannot be changed */

#define ENV_FLG_MASK		0xFF	/* mask of attributes we keep */
#define ENV_FLG_ADMIN		0x100	/* lets us internally override permissions */


/*  *********************************************************************
    *  _nvram_read(buffer,offset,length)
    *  
    *  Read data from the NVRAM device
    *  
    *  Input parameters: 
    *  	   buffer - destination buffer
    *  	   offset - offset of data to read
    *  	   length - number of bytes to read
    *  	   
    *  Return value:
    *  	   number of bytes read, or <0 if error occured
    ********************************************************************* */
static int
_nvram_read(unsigned char *nv_buf, unsigned char *buffer, int offset, int length)
{
    int i;
    if (offset > NVRAM_SIZE)
	return -1; 

    for ( i = 0; i < length; i++) {
	buffer[i] = ((volatile unsigned char*)nv_buf)[offset + i];
    }
    return length;
}


static char*
_strnchr(const char *dest,int c,size_t cnt)
{
	while (*dest && (cnt > 0)) {
	if (*dest == c) return (char *) dest;
	dest++;
	cnt--;
	}
	return NULL;
}



/*
 * Core support API: Externally visible.
 */

/*
 * Get the value of an NVRAM variable
 * @param	name	name of variable to get
 * @return	value of variable or NULL if undefined
 */

char* 
cfe_env_get(unsigned char *nv_buf, char* name)
{
    int size;
    unsigned char *buffer;
    unsigned char *ptr;
    unsigned char *envval;
    unsigned int reclen;
    unsigned int rectype;
    int offset;
    int flg;
    
    size = NVRAM_SIZE;
    buffer = &_nvdata[0];

    ptr = buffer;
    offset = 0;

    /* Read the record type and length */
    if (_nvram_read(nv_buf, ptr,offset,1) != 1) {
	goto error;
    }
    
    while ((*ptr != ENV_TLV_TYPE_END)  && (size > 1)) {

	/* Adjust pointer for TLV type */
	rectype = *(ptr);
	offset++;
	size--;

	/* 
	 * Read the length.  It can be either 1 or 2 bytes
	 * depending on the code 
	 */
	if (rectype & ENV_LENGTH_8BITS) {
	    /* Read the record type and length - 8 bits */
	    if (_nvram_read(nv_buf, ptr,offset,1) != 1) {
		goto error;
	    }
	    reclen = *(ptr);
	    size--;
	    offset++;
	}
	else {
	    /* Read the record type and length - 16 bits, MSB first */
	    if (_nvram_read(nv_buf, ptr,offset,2) != 2) {
		goto error;
	    }
	    reclen = (((unsigned int) *(ptr)) << 8) + (unsigned int) *(ptr+1);
	    size -= 2;
	    offset += 2;
	}

	if (reclen > size)
	    break;	/* should not happen, bad NVRAM */

	switch (rectype) {
	    case ENV_TLV_TYPE_ENV:
		/* Read the TLV data */
		if (_nvram_read(nv_buf, ptr,offset,reclen) != reclen)
		    goto error;
		flg = *ptr++;
		envval = (unsigned char *) _strnchr(ptr,'=',(reclen-1));
		if (envval) {
		    *envval++ = '\0';
		    memcpy(_valuestr,envval,(reclen-1)-(envval-ptr));
		    _valuestr[(reclen-1)-(envval-ptr)] = '\0';
#if 0			
		    printk(KERN_INFO "NVRAM:%s=%s\n", ptr, _valuestr);
#endif
		    if(!strcmp(ptr, name)){
			return _valuestr;
		    }
		    if((strlen(ptr) > 1) && !strcmp(&ptr[1], name))
			return _valuestr;
		}
		break;
		
	    default: 
		/* Unknown TLV type, skip it. */
		break;
	    }

	/*
	 * Advance to next TLV 
	 */
		
	size -= (int)reclen;
	offset += reclen;

	/* Read the next record type */
	ptr = buffer;
	if (_nvram_read(nv_buf, ptr,offset,1) != 1)
	    goto error;
	}

error:
    return NULL;

}

