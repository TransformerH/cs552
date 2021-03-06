#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "m.h"

/** MAXIMUM */
#define MAX_FILE_COUNT INODE_NUMBER
#define MAX_FILE_SIZE ((64*256)+(64*64*256)+256*8)
// For fd. Since the maximum file number is 1024. We assign it to be 1024.
// Put fd number to the fd_table.
struct fd *fd_table[1024];
struct Ramdisk *rd;

char dir[4] = "dir";
char reg[4] = "reg";


int init_fs() {
    printf("INIT\n");
    // INIT FILESYSTEM:
    rd = (struct Ramdisk *)malloc(sizeof(struct Ramdisk));
    printf("RAMDISK Size=%d\n", (int)sizeof(struct Ramdisk));
    // Setting up root
    SET_INODE_TYPE_DIR(0);
    //printf("type=%s\n", rd->ib[0].type);
    SET_INODE_SIZE(0, 0);
    //printf("size=%d\n", rd->ib[0].size);
    // DONT ALLOCATE PARTITION BLOCK FOR ROOT NOW. DO IT WHEN IT IS NEEDED
    INIT_FREEINODE;
    INIT_FREEBLOCK;
    DECR_FREEINODE; // -1 for ROOT
    // EOF init
}

int is_block_empty(union Block *blk) {
	struct Block_reg re = blk->reg;
	int i;
	for (i = 0; i < BLOCK_SIZE; i++) {
		if (re.byte[i] != 0)
			return 0;
	}
	return 1;
}
//searches a parent directory for a file's inode, return -1 if not found
int get_inode_index (int node, char *pathname) {
	int i,k,j,z;
	struct Inode inode;
	struct Block_dir bd;
	struct Block_ptr bp;
	union Block *blk, *dblk;
	inode = GET_INODE_BY_INDEX(node);
	if (!strcmp(inode.type, "reg"))
		return -1;
	for (i = 0; i < 8; i++) {
		if (inode.blocks[i] != 0)
			bd = inode.blocks[i]->dir;
		else
			continue;
		for (k = 0; k < 16; k++) {
			if(!strcmp(bd.ent[k].filename, pathname))
				return bd.ent[k].inode_number;
		}
	}
	for (i = 8; i < 10; i++) {
		if (inode.blocks[i] != 0)
			bp = inode.blocks[i]->ptr;
		else
			continue;
		for (k = 0; k < BLOCK_SIZE/4; k++) {
			blk = bp.blocks[k];
			if (blk != 0)
				bd = blk->dir;
			else
				continue;
			if (i == 8) {
				for (j = 0; j < 16; j++) {
					if(!strcmp(bd.ent[j].filename, pathname)) {
						return bd.ent[j].inode_number;
					}
				} 				
			}
			else {
				for (j = 0; j < BLOCK_SIZE/4; j++) {
					dblk = blk->ptr.blocks[j];
					if (dblk != 0)
						bd = dblk->dir;
					else
						continue;
					for (z = 0; z < 16; z++) {
						if(!strcmp(bd.ent[z].filename, pathname)) {
							return bd.ent[z].inode_number;
						}
					}
				}
			}
		}		
	}
	//fieldname not found
	return -1;
}


//checks if pathname exists, -1 if error, 0 if does not exists, >0 if does exists
int check_pathname (char *pathname, char* last, short* super_inode) {
    char name[14];
	char *slash;
	unsigned int size;
	// struct Inode inode;
	int node_index = 0;
	int current_index;
	// unsigned int dir = 0;
	memset(name, 0, 14);
    //validate that user inputed root
	if (pathname[0] != '/')
		return -1;
	size = strnlen(pathname, 100);
	// if size is 1, then the user only entered '/'
	if (size < 2)
		return -1;
	pathname++;
	while ((slash = strchr(pathname, '/')) != NULL) {
		size = slash-pathname;
		// check if someone typed two slash in row (i.e //)
		if (size < 1)
			return -1;
		// check if fieldname is greater than 14 chars
		if (size > 14)
			return -1;
		strncpy(name, pathname, size);
		name[size] = '\0';
		pathname = slash + 1;
		node_index = get_inode_index(node_index, name);
		if (node_index < 0)
			return -1;
	}
	//means the last character is a / (could use this to create new dir) or just error out
	if (*pathname == '\0')
		return -1;
		// dir = 1;
	//copy the specified name in final dir to name
	strncpy(name, pathname, 14);
	current_index = get_inode_index(node_index, name);
	//set last as the final file name
	strncpy(last, name, 14);
	*super_inode = node_index;
	//if returns something other than -1, it means that this pathname already exits
	if (current_index > 0)
		return current_index;
	return 0;
}
//recursive search
int recursive_inode_search(short *array, int *size, short cnode, short tnode) {
	int i,k,j,z; 
	struct Inode inode;
	struct Block_dir bd;
	struct Block_ptr bp;
	union Block *blk, *dblk;
	inode = GET_INODE_BY_INDEX(cnode);
	if (!strcmp(inode.type, "reg"))
		return -1;
	for (i = 0; i < 8; i++) {
		if (inode.blocks[i] != 0)
			bd = inode.blocks[i]->dir;
		else
			continue;
		for (k = 0; k < 16; k++) {
			if(bd.ent[k].inode_number == tnode)
				return 1;
			else {
				array[*size] = bd.ent[k].inode_number;
				*size+=1;
				if (recursive_inode_search(array, size, bd.ent[k].inode_number, tnode) == 1)
					return 1;
				*size-=1;
				array[*size] = 0;
			}
				
		}
	}
	for (i = 8; i < 10; i++) {
		if (inode.blocks[i] != 0)
			bp = inode.blocks[i]->ptr;
		else
			continue;
		for (k = 0; k < BLOCK_SIZE/4; k++) {
			blk = bp.blocks[k];
			if (blk != 0)
				bd = blk->dir;
			else
				continue;
			if (i == 8) {
				for (j = 0; j < 16; j++) {
					if(bd.ent[j].inode_number == tnode) {
						return 1;
					}
					else {
						array[*size] = bd.ent[j].inode_number;
						*size+=1;
						if (recursive_inode_search(array, size, bd.ent[j].inode_number, tnode) == 1)
							return 1;
						*size-=1;
						array[*size] = 0;
					}
				} 				
			}
			else {
				for (j = 0; j < BLOCK_SIZE/4; j++) {
					dblk = blk->ptr.blocks[j];
					if (dblk != 0)
						bd = dblk->dir;
					else
						continue;					
					for (z = 0; z < 16; z++) {
						if(bd.ent[z].inode_number == tnode) {
							return 1;
						}
						else {
							array[*size] = bd.ent[j].inode_number;
							*size+=1;
							if (recursive_inode_search(array, size, bd.ent[z].inode_number, tnode) == 1)
								return 1;
							*size-=1;
							array[*size] = 0;
						}
					}
				}
			}
		}		
	}
	//node does not exist here
	return 0;
}
//exhaustive search to find
int parent_inode_index_trace(short inode, short *trace) {
	int size = 1, node = 0, found = 0;
	//allocate minimum space for array
	//root inode will always be 0
	short fotrace[1024];
	fotrace[0] = 0;
	found = recursive_inode_search(fotrace, &size, 0, inode);
	if (!found)
		return -1; //fieldname not found
	memcpy(trace, fotrace, size * sizeof(short));
	return size;
}

int recursive_inode_size_add(short inode, int size) {
	short parent_array[1024];
	int array_size, i, t;
	array_size = parent_inode_index_trace(inode, parent_array);
	if (array_size < 0)
		return -1;
	for (i = 0; i < array_size; i++) {
		t = GET_INODE_SIZE(parent_array[i]);
		SET_INODE_SIZE(parent_array[i], t+size);
	}
	SET_INODE_SIZE(inode, size);
	return 0;
}
int delete_dir_entry(short node, char *pathname) {
	int i,k,j,z;
	struct Inode *inode;
	struct Block_dir *bd;
	struct Block_ptr *bp;
	union Block *blk, *dblk;
	inode = &(GET_INODE_BY_INDEX(node));
	if (!strcmp(inode->type, "reg"))
		return -1;
	for (i = 0; i < 8; i++) {
		if (inode->blocks[i] != 0)
			bd = &(inode->blocks[i]->dir);
		else
			continue;
		for (k = 0; k < 16; k++) {
			if(!strcmp(bd->ent[k].filename, pathname)) {
				memset(&(bd->ent[k]), 0, sizeof(struct Dir_entry));
				if(is_block_empty(inode->blocks[i])) {
					SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(inode->blocks[i]));
					INCR_FREEBLOCK;
					inode->blocks[i] = NULL;					
				}
				return 0;
			}
				
		}
	}
	for (i = 8; i < 10; i++) {
		if (inode->blocks[i] != 0)
			bp = &(inode->blocks[i]->ptr);
		else
			continue;
		for (k = 0; k < BLOCK_SIZE/4; k++) {
			if ((blk = bp->blocks[k]) != 0)
				bd = &(blk->dir);
			else
				continue;
			if (i == 8) {
				for (j = 0; j < 16; j++) {
					if(!strcmp(bd->ent[j].filename, pathname)) {
						memset(&(bd->ent[j]), 0, sizeof(struct Dir_entry));
						if(is_block_empty(blk)) {
							SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(blk));
							INCR_FREEBLOCK;
							bp->blocks[k] = NULL;					
						}
						break;
					}	
				}
				if(is_block_empty(inode->blocks[i])) {
					SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(inode->blocks[i]));
					INCR_FREEBLOCK;
					inode->blocks[i] = NULL;					
				}
				return 0; 				
			}
			else {
				for (j = 0; j < BLOCK_SIZE/4; j++) {
					if((dblk = blk->ptr.blocks[j]) != 0)
						bd = &(dblk->dir);					
					else
						continue;
					for (z = 0; z < 16; z++) {
						if(!strcmp(bd->ent[z].filename, pathname)) {
							memset(&(bd->ent[z]), 0, sizeof(struct Dir_entry));
							if(is_block_empty(dblk)) {
								SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(dblk));
								INCR_FREEBLOCK;
								blk->ptr.blocks[j] = NULL;					
							}
							break;
						}
					}
					if(is_block_empty(blk)) {
						SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(blk));
						INCR_FREEBLOCK;
						bp->blocks[k] = NULL;					
					}
					if(is_block_empty(inode->blocks[i])) {
						SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(inode->blocks[i]));
						INCR_FREEBLOCK;
						inode->blocks[i] = NULL;					
					}
					return 0;
				}
			}
		}		
	}
	//fieldname not found
	return -1;
}
int recursive_pathname_size_decr(char *pathname, int b_size) {
	char name[14];
	char *slash;
	unsigned int size;
	int rc;
	// struct Inode inode;
	int node_index = 0;
	int current_index;
	// unsigned int dir = 0;
	memset(name, 0, 14);
    //validate that user inputed root
	if (pathname[0] != '/')
		return -1;
	size = strnlen(pathname, 100);
	// if size is 1, then the user only entered '/'
	if (size < 2)
		return -1;
	pathname++;
	SET_INODE_SIZE(node_index, (GET_INODE_SIZE(node_index)-b_size));	
	while ((slash = strchr(pathname, '/')) != NULL) {
		size = slash-pathname;
		// check if someone typed two slash in row (i.e //)
		if (size < 1)
			return -1;
		// check if fieldname is greater than 14 chars
		if (size > 14)
			return -1;
		strncpy(name, pathname, size);
		name[size] = '\0';
		pathname = slash + 1;
		node_index = get_inode_index(node_index, name);
		if (node_index < 0)
			return -1;
		SET_INODE_SIZE(node_index, (GET_INODE_SIZE(node_index)-b_size));
	}
	//means the last character is a / (could use this to create new dir) or just error out
	if (*pathname == '\0')
		return -1;
		// dir = 1;
	//copy the specified name in final dir to name
	strncpy(name, pathname, 14);
	rc = delete_dir_entry(node_index, name);
	return rc;	
}

int find_free_block() {
    // Find a new free block, using first-fit. NOTE: before returning, we SET THE BITMAP = 1 !!!!!!
    if (SHOW_FREEBLOCK == 0) {
        printf("Find Free Block: FREEBLOCK=0, returning -1 \n");
        return -1;
    } else {
        int j = 0;
        for (j; j < PARTITION_NUMBER; j++) {
            if (SEE_BITMAP_FREE(j) == 1) {
                printf("FREE BLOCK = %d\n", j);
                // Set bitmap = 1
                SET_BITMAP_ALLOCATE_BLOCK(j);
                DECR_FREEBLOCK;
                return j;
            }
        }
    }
    printf("No free block found, ERROR\n");
    return -1;
}

int find_free_inode() {
    // Find a new free block, using first-fit.
    if (SHOW_FREEINODE == 0) {
        printf("Find Free inode: FREEINODE=0, returning -1 \n");
        return -1;
    } else {
        int j = 0;
        for (j; j < MAX_FILE_COUNT; j++) {
            int f = rd->ib[j].type[0];
            if (f == 0) {
                DECR_FREEINODE;
                return j;
            }
        }
    }
    printf("No free block found, ERROR\n");
    return -1;
}
#define debug
//#define nofree
int insert_inode(short super_inode, short new_inode, char *filename) {
    // Loop through super_inode LOC ptr.
    int i = 0; // Location blocks
    while(i < 10) {
        // If there is allocated block?
        if (GET_INODE_LOCATION_BLOCK(super_inode, i) == 0) {
            //printf("i = %d, is empty\n", i);
            // Allocate partition blocks for super_inode
            int fb = find_free_block();
            // Assign free block to super_inode location i.
            SET_INODE_LOCATION_BLOCK(super_inode, i, fb);
            if (i >= 0 && i <= 7) {
                // 1~ 7 is direct block
                SET_DIR_ENTRY_NAME(fb, 0, filename);
                SET_DIR_ENTRY_INODE(fb, 0, new_inode);
#ifdef debug
                printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Dir_entry index = %d\n", super_inode, i, 0);
                PRINT_DIR_ENTRY(fb, 0);
#endif
                return 1;
            } else if (i == 8) {
                // 8 is single redirection block
                // Treat previously allocated block as a ptr block
                // 1. Allocate another partition block for directory block
                int fb2 = find_free_block();
                // 2. Assign new block index[8].blocks[0];
                ASSIGN_LOCATION_SINGLE_RED(super_inode, 0, fb2);
                SET_DIR_ENTRY_NAME(fb2, 0, filename);
                SET_DIR_ENTRY_INODE(fb2, 0, new_inode);
#ifdef debug
                printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Ptr_entry = %d, Dir_entry index = %d\n", super_inode, i, 0, 0);
#endif
                return 1;
            } else if (i == 9) {
                // 9 is double redirection block
                // Treat previously allocated block as a ptr block
                // 1. Allocate another partition block for 2nd ptr block
                int fb2 = find_free_block();
                // 2. Assign new block index[9].blocks[0];
                ASSIGN_LOCATION_DOUBLE_FST_RED(super_inode, 0, fb2);
                // 3. Allocate another partition block for directory block.
                int fb3 = find_free_block();
                ASSIGN_LOCATION_DOUBLE_SND_RED(super_inode, 0, 0, fb3);
                SET_DIR_ENTRY_NAME(fb3, 0, filename);
                SET_DIR_ENTRY_INODE(fb3, 0, new_inode);
#ifdef debug
                printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Ptr_entry1 = %d, PTR_ENT2=%d, Dir_entry index = %d\n", super_inode, i, 0, 0, 0);
#endif
                return 1;
            }
        } else {
            // Block already allocated. Loop though all blocks to find a free entry
            // First-fit here as well. 
            if (i >= 0 && i <= 7) {
                // 1~ 7 is direct block
                int j = 0;
                for (j; j < 16; j++) {
                    if (GET_INODE_FROM_INODE_LOCATION_INODE(super_inode, i, j) == 0) {
                        // We have a free location
#ifdef debug
                        printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Dir_entry index = %d\n", super_inode, i, j);
#endif
                        // Insert inode into this location
                        SET_INODE_FROM_INODE_LOCATION_INODE(super_inode, i, j, new_inode);
                        SET_INODE_FROM_INODE_LOCATION_FILENAME(super_inode, i, j, filename);
                        PRINT_INODE_FROM_INODE_LOCATION(super_inode, i, j);
                        return 1;
                    }
#ifdef nofree
                    printf("No free in #%d direct block\n", i);
#endif
                }
#ifdef nofree
                printf("No free in ALL direct block\n");
#endif
            } else if (i == 8) {
                // 8 is single redirection block
                int j = 0;
                for (j; j < (256/4); j++) {
                    // Loop through the redirection block, j is PTR_ENTRY
                    if (GET_INODE_LOCATION_BLOCK_SIN(super_inode, j) == 0) {
                        // If it equals 0, then there are NO Dir block allocated.
                        // 1. Allocate a new Partition block for Dir block.
                        int fb = find_free_block();
                        // 2. Assign new block index[8].blocks[j];
                        ASSIGN_LOCATION_SINGLE_RED(super_inode, j, fb);
                        SET_DIR_ENTRY_NAME(fb, 0, filename);
                        SET_DIR_ENTRY_INODE(fb, 0, new_inode);
#ifdef debug
                        printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Ptr_entry = %d, Dir_entry index = %d\n", super_inode, i, j, 0);
                        PRINT_INODE_FROM_INODE_LOCATION_SIN(super_inode, j, 0);
#endif
                        return 1;
                    } else {
                        // A dir block is present, loop through to find free location
                        int k = 0;
                        for (k; k < 16; k++) {
                            // Loop through the Dir Block
                            if (GET_INODE_FROM_INODE_LOCATION_SIN_INODE(super_inode, j, k) == 0) {
#ifdef debug
                            printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Ptr_entry = %d, Dir_entry index = %d\n", super_inode, i, j, k);
#endif
                            SET_INODE_FROM_INODE_LOCATION_SIN_INODE(super_inode, j, k, new_inode);
                            SET_INODE_FROM_INODE_LOCATION_SIN_FILENAME(super_inode, j, k, filename);
                            PRINT_INODE_FROM_INODE_LOCATION_SIN(super_inode, j, k);
                            return 1;
                            }
                        }
                    }
#ifdef nofree
                    printf("No free in PTR_ENT = %d, single redirection block\n", j);
#endif
                }
#ifdef nofree
                printf("No free in ALL single redirection block\n");
#endif
            } else if (i == 9) {
                // 9 is double redirection block
                int j = 0;
                for (j; j < (256/4); j++) {
                    // Loop through the first redirection block, j is PTR_ENT1
                    if (GET_INODE_LOCATION_BLOCK_DOB_FST(super_inode, j) == 0) {
                        // There is no second redirection block.
                        // 1. Find free block and Assign Second redirection block.
                        int fb = find_free_block();
                        ASSIGN_LOCATION_DOUBLE_FST_RED(super_inode, j, fb);
                        // 2. Find free block and Assign to Sec for Directory block.
                        int fb2 = find_free_block();
                        ASSIGN_LOCATION_DOUBLE_SND_RED(super_inode, j, 0, fb2);
                        // 3. Insert new inode
                        SET_INODE_FROM_INODE_LOCATION_DOB_INODE(super_inode, j, 0, 0, new_inode);
                        SET_INODE_FROM_INODE_LOCATION_DOB_FILENAME(super_inode, j, 0, 0, filename);
#ifdef debug
                        printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Ptr_entry1 = %d, PTR_ENT2=%d, Dir_entry index = %d\n", super_inode, i, j, 0, 0);
                        PRINT_INODE_FROM_INODE_LOCATION_DOB(super_inode, j, 0, 0);
#endif
                        return 1;
                    } else {
                        // There is a second redirection block.
                        int k = 0;
                        for (k; k < (256/4); k++) {
                            // Loop through the second redirection block, k is PTR_ENT2
                            if (GET_INODE_LOCATION_BLOCK_DOB_SND(super_inode, j, k) == 0) {
                                // There is no Directory block
                                // 1. Find free block and Assign to second redirection block.
                                int fb = find_free_block();
                                ASSIGN_LOCATION_DOUBLE_SND_RED(super_inode, j, k, fb);
                                // 2. Insert new inode
                                SET_INODE_FROM_INODE_LOCATION_DOB_INODE(super_inode, j, k, 0, new_inode);
                                SET_INODE_FROM_INODE_LOCATION_DOB_FILENAME(super_inode, j, k, 0, filename);
#ifdef debug
                                printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Ptr_entry1 = %d, PTR_ENT2=%d, Dir_entry index = %d\n", super_inode, i, j, k, 0);
                                PRINT_INODE_FROM_INODE_LOCATION_DOB(super_inode, j, k, 0);
#endif
                                return 1;
                            } else {
                                // There is a Directory block
                                int l = 0;
                                for (l; l < 16; l++) {
                                    // l is ENT
                                    // Loop through Directory block to find a free entry
                                    if (GET_INODE_FROM_INODE_LOCATION_DOB_INODE(super_inode, j, k, l) == 0) {
                                        SET_INODE_FROM_INODE_LOCATION_DOB_INODE(super_inode, j, k, l, new_inode);
                                        SET_INODE_FROM_INODE_LOCATION_DOB_FILENAME(super_inode, j, k, l, filename);
#ifdef debug
                                        printf("**New inode inserted**\n ----To free location. super_inode = %d, Location index = %d, Ptr_entry1 = %d, PTR_ENT2=%d, Dir_entry index = %d\n", super_inode, i, j, k, l);
                                        PRINT_INODE_FROM_INODE_LOCATION_DOB(super_inode, j, k, l);
#endif
                                        return 1;
                                    }
                                }
                            }
#ifdef nofree
                            printf("No free in PTR_ENT1=%d, PTR_ENT2=%d double redirection block\n", j, k);
#endif
                        }
                    }
#ifdef nofree
                    printf("No free in PTR_ENT1=%d double redirection block\n", j);
#endif
                }
#ifdef nofree
                printf("No free in ALL double redirection block\n");
#endif
            }
        }
        i++;
    }
    // cannot find a free directory entry
    return -1;
}

#undef debug
#undef nofree
int kcreat(char *pathname) {
    PRINT_FREEINODE_COUNT;
    PRINT_FREEBLOCK_COUNT;
    // kernel creat. Create a file
    int fi = find_free_inode();
    printf("Free inode = %d\n", fi);
    if (fi < 0) {
        return -1;
    }
    // Check pathname and get last entry.
    char *last = (char *)malloc(14);
    short super_inode;
    if (check_pathname(pathname, last, &super_inode) == -1) {
        // Pathname failed. 
        return -1;
    }
    printf("%s, super=%d\n", last, super_inode);
    
    // Create file
    // 1. Find a new free inode. DONE ABOVE
    // 2. Assign type as reg. size=0. DO NOT ALLOCATE NEW BLOCK.
    SET_INODE_TYPE_REG(fi);
    SET_INODE_SIZE(fi, 0);
    // 3. Assign new inode to super inode.
    if (insert_inode(super_inode, fi, last) != 1) {
        return -1;
    }
    return 0;
}

int kmkdir(char *pathname) {
    // kernel mkdir. Create a DIR
    int fi = find_free_inode();
    printf("Free inode = %d\n", fi);
    if (fi == -1) {
        return -1;
    }
    printf("%s\n", pathname);

    
    // Check_pathname and get last entry
    char *last = (char *)malloc(14);
    short super_inode;

    if (check_pathname(pathname, last, &super_inode) != 0) {
        // Pathname failed.
		printf("pathname: %s, already exists\n", pathname);
        return -1;
    }
    printf("%s, super=%d\n", last, super_inode);
    
#ifdef debug
    int i = 0;
    for (i; i<14; i++) {
        printf("%c ", last[i]);
    }
#endif
    // Create directory
    // 1. Find a new free inode. DONE ABOVE
    // 2. Assign type as dir. size=0. DO NOT ALLOCATE NEW BLOCK.
    SET_INODE_TYPE_DIR(fi);
    SET_INODE_SIZE(fi, 0);
    // 3. Assign new inode to super inode.
    if (insert_inode(super_inode, fi, last) != 1) {
        return -1;
    }
    return 0;
}

int build_inode_structure(short inode, unsigned char *ist) {
    /**
     * This function can be use for kread, kwrite, kreaddir
     * build the entire inode structure first, then go to offest. 
     */
    // Traverse through all possible location
    int i = 0; // Location blocks
    int position = 0; // Position can also be used as a counter for size.
    while(i < 10) {
        // If there is allocated block?
        if (GET_INODE_LOCATION_BLOCK(inode, i) == 0) {
            //printf("i = %d, is empty\n", i);
            // No allocated block at i, we can return.
            return position;
        } else {
            // Block already allocated. Loop though all blocks and build inode structure
            if (i >= 0 && i <= 7) {
                // 1~ 7 is direct block
                // Since there is a block allocated, we can just copy the entire block.
                memcpy((ist + position), GET_INODE_LOCATION_BLOCK(inode, i), 256);
                position += 256;
            } else if (i == 8) {
                // 8 is single redirection block
                int j = 0;
                for (j; j < (256/4); j++) {
                    // Loop through the redirection block, j is PTR_ENTRY
                    if (GET_INODE_LOCATION_BLOCK_SIN(inode, j) == 0) {
                        // If it equals 0, then there are NO Dir block allocated.
                        return position;
                    } else {
                        // A dir block is allocated
                        memcpy((ist + position), GET_INODE_LOCATION_BLOCK_SIN(inode, j), 256);
                        position += 256;
                    }
                }
            } else if (i == 9) {
                // 9 is double redirection block
                int j = 0;
                for (j; j < (256/4); j++) {
                    // Loop through the first redirection block, j is PTR_ENT1
                    if (GET_INODE_LOCATION_BLOCK_DOB_FST(inode, j) == 0) {
                        // There is no second redirection block.
                        return position;
                    } else {
                        // There is a second redirection block.
                        int k = 0;
                        for (k; k < (256/4); k++) {
                            // Loop through the second redirection block, k is PTR_ENT2
                            if (GET_INODE_LOCATION_BLOCK_DOB_SND(inode, j, k) == 0) {
                                // There is no Directory block
                                return position;
                            } else {
                                // There is a Directory block
                                memcpy((ist + position), GET_INODE_LOCATION_BLOCK_DOB_SND(inode, j, k), 256);
                                position += 256;
                            }
                        }
                    }
                }
            }
        }
        i++;
    }
    return position;
}

int kopen(char *pathname) {
#ifdef debug
    printf("%s\n", pathname);
#endif
    // Check_pathname and get last entry
    char *last = (char *)malloc(14);
    short super_inode;
	int inode;
    if ((inode = check_pathname(pathname, last, &super_inode)) < 1) {
        // Pathname failed.
        return -1;
    }
    // 1. Find inode number for file within super_inode
    // int temp_inode = 0;

    // int inode = temp_inode;
    // 2. Check if fd_table is active. 
    if (fd_table[inode] == NULL) {
        struct fd* newfd;
        newfd = (struct fd *)malloc(sizeof(struct fd));
        newfd->inode = &rd->ib[inode];
        fd_table[inode] = newfd;
    } else {
        // Other file is accessing it.
        return -1;
    }
#ifdef debug
    printf("%d, %d, %s\n", fd_table[inode]->read_pos, fd_table[inode]->write_pos, fd_table[inode]->inode->type);
#endif
    
    // We return the inode index of the file, which also is the fd_table index.
    // Since there is no special requirement on the traditional linux incrementing fd (i.e. first fd is 1, second is 2...etc), we just use what is easiest. 
    return inode;
}

int kclose(int fd) {
    // Again, fd=inode index
    if (fd_table[fd] == NULL) {
        // Not an open file
        return -1;
    } else {
		free(fd_table[fd]);
		fd_table[fd] = NULL;
    }
    return 0;
}

int read_file(short inode, int read_pos, int num_bytes, unsigned char *temp) {
    // Build the inode structure first.
    unsigned char *ist = (unsigned char *)malloc(MAX_FILE_SIZE);
    int size = build_inode_structure(inode, ist);
//#ifdef debug
    printf("size = %d\n", size);
//#endif
    if (size == 0) {
        return 0;
    }
    if (read_pos >= (size - 1)) {
        // Read_pos is greater then possible size
        return -1;
    }
    if ((read_pos + num_bytes) > (size - 1)) {
        // Not enough bytes for us to read, read what is possible.
        memcpy(temp, ist + read_pos, (size - read_pos));
        // Increment fd position
        fd_table[inode]->read_pos = size;

        return (size - read_pos);
    } else {
        // Enough byte for us to read.
        memcpy(temp, ist + read_pos, num_bytes);
        // Increment fd position
        fd_table[inode]->read_pos = read_pos + num_bytes;
        return num_bytes;
    }
    // Error if reach here.
    return -1;
}

int kread(int fd, char *address, int num_bytes) {
    // Again, fd=inode index
    if (fd_table[fd] == NULL) {
        // Not an valid fd
        return -1;
    }
    // Check if it is a regular file
    if (memcmp(reg, GET_INODE_TYPE(fd), 3) == 0) {
        // Read num_bytes TO ADDRESS location
        /*
        unsigned char *temp = (unsigned char *)malloc(sizeof(num_bytes));
        int ret = read_file(fd, fd_table[fd]->read_pos, num_bytes, temp);
        if (ret == -1) {
            return -1;
        }
         if (ret == 0) {
         // no byte read
         return 0;
         }
         memcpy(address, temp, ret);
         */
        
        
        // TEST ----- NOT WORKING. some bugs here. 
        int second_redir = 8*256;
        unsigned char *temp = (unsigned char *)malloc(second_redir);
        int pos = 0;
        int ret = 0;
        while (pos < num_bytes) {
            int bytes = 0;
            if ((num_bytes - pos) <= second_redir) {
                bytes = num_bytes - pos;
            } else {
                bytes = second_redir;
            }
            
            int ret2 = read_file(fd, fd_table[fd]->read_pos, bytes, temp);
            //printf("pos = %d, bytes=%d, num_bytes = %d, ret2= %d\n", pos, bytes, num_bytes, ret2);
            if (ret2 == -1) {
                return -1;
            }
            if (ret2 == 0) {
                // no byte read
                return 0;
            }
            //printf("pos = %d, bytes=%d, num_bytes = %d, ret2= %d\n", pos, bytes, num_bytes, ret2);
            memcpy(address + pos, temp, bytes);
            //memset(temp, 0, bytes);
            pos += bytes;
            ret += ret2;
            //printf("pos = %d, bytes=%d, num_bytes = %d, ret2= %d\n", pos, bytes, num_bytes, ret2);
        }
        free(temp);
        // My test end

        // COPY TO USER SPACE
        // return number of bytes actually read
        return ret;
    } else {
        // Not a regular file
        return -1;
    }
}

int check_for_last_write(int size, int ptr_count, unsigned char *file, int fb) {
    if ((size - ptr_count) <=  256) {
        // To prevent garbage in our partition block.
        // NOTE: Since we know the inode size (for reading), we can skip this step
        // but to keep our partition clean, lets do it anyway.
        unsigned char temp[256];
        memcpy(temp, &file[ptr_count], (size - ptr_count));
        WRITE_TO_FILE(fb, temp);
        return 1;
    }
    return -1;
}

int write_to_fs(short inode, unsigned char *ist, int new_size) {
	int fb, fb2, fb3;
	int i, j, k, size, position = 0;
	for (i = 0; i < 10; i++) {
		//check to see if current block is allocated already
		if (GET_INODE_LOCATION_BLOCK(inode, i) == 0) {        
			//current block is not allocated, find free block and assign it to current index
			fb = find_free_block();
			ASSIGN_LOCATION(inode, i, fb);
		}
		if (i < 8) {
			//if it is a direct block, then can comense write
			if ((size = new_size - position) > 256)
				size = 256;
			WRITE_TO_LOCATION(inode, i, (ist + position), size);
            position += size;				
			//check to see whether or not this is last block
			if ((new_size - position) < 256)
				return 1;
		}
		else {
			for (j = 0; j < BLOCK_SIZE/4; j++) {
				//check to see if first indirection is allocated
				if (GET_INODE_LOCATION_BLOCK_GENERIC_FST(inode, i, j) == 0) {
					//if not, find free block and assign
					fb2 = find_free_block();
					ASSIGN_LOCATION_GENERIC_RED(inode, i, j, fb2);
				}
				if (i == 8) {
					if ((size = new_size - position) > 256)
						size = 256;
					//begin write to 1st indirection block
					WRITE_TO_LOCATION_SINGLE_RED(inode, j, (ist + position), size);
                    position += size;
					//check to see whether or not this is last block
					if ((new_size - position) < 256)
						return 1;
				}
				else if(i == 9) {
					for (k = 0; k < BLOCK_SIZE/4; k++) {
						if (GET_INODE_LOCATION_BLOCK_DOB_SND(inode, j, k) == 0) {
							fb3 = find_free_block();
							ASSIGN_LOCATION_DOUBLE_SND_RED(inode, j, k, fb3);
						}
						if ((size = new_size - position) > 256)
							size = 256;
                        WRITE_TO_LOCATION_DOUBLE_RED(inode, j, k, (ist + position), size);
                        position += size;
						//check to see whether or not this is last block
						if ((new_size - position) < 256)
							return 1;				
					}					
				}
			}
		}
	}
	//unable to fill all the data
	return -1;
}
#define debug
int write_file(short inode, int write_pos, int num_bytes, unsigned char *temp) {
    // Build the inode structure first.
    unsigned char *ist = (unsigned char *)malloc(MAX_FILE_SIZE);
    int size = build_inode_structure(inode, ist);
#ifdef debug
    printf("size = %d\n", size);
#endif
    if ((write_pos + num_bytes) > MAX_FILE_SIZE) {
        // Not enough bytes for us to write, write what is possible.
        memcpy(ist + write_pos, temp, (MAX_FILE_SIZE - write_pos));
        // Actually write back to filesystem
        write_to_fs(inode, ist, MAX_FILE_SIZE);
        // Set Inode new size
		recursive_inode_size_add(inode, MAX_FILE_SIZE);		
        // SET_INODE_SIZE(inode, MAX_FILE_SIZE);
        // Increment fd position
        fd_table[inode]->write_pos = MAX_FILE_SIZE;
		free(ist);
        return (MAX_FILE_SIZE - write_pos);
    } else {
        // Enough byte for us to read.
        memcpy(ist + write_pos, temp, num_bytes);
        // Actually write back to filesystem
        write_to_fs(inode, ist, write_pos + num_bytes);
        // Set new size for inode and recursively for all parent dirs		
		recursive_inode_size_add(inode, write_pos + num_bytes);
        // SET_INODE_SIZE(inode, write_pos + num_bytes);
        // Increment fd position
        fd_table[inode]->write_pos = write_pos + num_bytes;
		free(ist);
        return num_bytes;
    }
    // Error if reach here.
    return -1;
}

int kwrite(int fd, char *address, int num_bytes) {
    // Again, fd=inode index
    if (fd_table[fd] == NULL) {
        // Not an valid fd
        return -1;
    }
    // Check if it is a regular file
    if (memcmp(reg, GET_INODE_TYPE(fd), 3) == 0) {
        // write num_bytes From ADDRESS location
        // COPY FROM USERSPACE
        int second_redir = 256*8;
        /*
        unsigned char *temp = (unsigned char *)malloc(sizeof(num_bytes));
        memcpy(temp, address, num_bytes);
        printf("test\n");
        int ret = write_file(fd, fd_table[fd]->write_pos, num_bytes, temp);
		free(temp);
        */
        unsigned char *temp = (unsigned char *)malloc(second_redir);
        int pos = 0;
        int ret = 0;
        while (pos < num_bytes) {
            int bytes = 0;
            if ((num_bytes - pos) <= second_redir) {
                bytes = num_bytes - pos;
            } else {
                bytes = second_redir;
            }
            
            memcpy(temp, address + pos, bytes);
            printf("test\n");
            int ret2 = write_file(fd, fd_table[fd]->write_pos, bytes, temp);
            pos += bytes;
            ret += ret2;
            memset(temp, 0, bytes);
            if (ret2 == -1) {
                return -1;
            }
        }
        printf("total size=%d\n", (*fd_table[fd]->inode).size);
        free(temp);
        
        /// END OF TESTING
        
        if (ret == -1) {
            return -1;
        }
        if (ret == 0) {
            // no byte write
            return 0;
        }
        
        // return number of bytes actually write
        return ret;
    } else {
        // Not a regular file
        return -1;
    }
}

int klseek(int fd, int offset) {
    // Again, fd=inode index
    if (fd_table[fd] == NULL) {
        // Not an valid fd
        return -1;
    }
    // Check if it is a regular file
    if (memcmp(reg, GET_INODE_TYPE(fd), 3) == 0) {
        // Assuming setting both read and write file position.
        //printf("offset=%d\n", offset);
        fd_table[fd]->read_pos = offset;
        fd_table[fd]->write_pos = offset;
        // returning the new position
        return fd_table[fd]->read_pos = offset;
    } else {
        // Not a regular file
        return -1;
    }
}

int unlink_file(int inode) {
    int i = 0;
	int k,j;
    while(i < 10) {
        if (GET_INODE_LOCATION_BLOCK(inode, i) == 0) {
            // No allocated block at i. Nothing to free
            return 1;
        } else {
            // Allocated block at i.
            if (i >= 0 && i <= 7) {
                // Remove File Block
				memset(GET_INODE_LOCATION_BLOCK(inode, i)->reg.byte, 0, 256);
				SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(GET_INODE_LOCATION_BLOCK(inode, i)));
				INCR_FREEBLOCK;
                
            } else if (i == 8) {
				for (k = 0; k < BLOCK_SIZE/4; k++) {
					if(GET_INODE_LOCATION_BLOCK_SIN(inode, k) == 0)
						break;
					memset(GET_INODE_LOCATION_BLOCK_SIN(inode, k)->reg.byte, 0, 256);
					SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(GET_INODE_LOCATION_BLOCK_SIN(inode, k)));
					INCR_FREEBLOCK;
                    
				}
				memset(GET_INODE_LOCATION_BLOCK(inode, i)->reg.byte, 0, 256);
				SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(GET_INODE_LOCATION_BLOCK(inode, i)));
				INCR_FREEBLOCK;
				// Remove All File Blocks
                // Then Remove Single redirection block
            } else if (i == 9) {
				for (k = 0; k < BLOCK_SIZE/4; k++) {
					if(GET_INODE_LOCATION_BLOCK_DOB_FST(inode, k) == 0)
						break;
					for (j = 0; j < BLOCK_SIZE/4; j++) {
						if(GET_INODE_LOCATION_BLOCK_DOB_SND(inode, k, j) == 0)
							break;
						memset(GET_INODE_LOCATION_BLOCK_DOB_SND(inode, k, j)->reg.byte, 0, 256);
						SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(GET_INODE_LOCATION_BLOCK_DOB_SND(inode, k, j)));
						INCR_FREEBLOCK;
                    }
                    
					memset(GET_INODE_LOCATION_BLOCK_DOB_FST(inode, k)->reg.byte, 0, 256);
					SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(GET_INODE_LOCATION_BLOCK_DOB_FST(inode, k)));
					INCR_FREEBLOCK;
                    
				}
				memset(GET_INODE_LOCATION_BLOCK(inode, i)->reg.byte, 0, 256);
				SET_BITMAP_FREE_BLOCK(GET_BLOCK_INDEX_PARTITION(GET_INODE_LOCATION_BLOCK(inode, i)));
				INCR_FREEBLOCK;
                // Remove All File Blocks
                // Then Remove All Second Double redirection blocks
                // Then Remove First double redirection block
            }
        }
        i++;
    }
    return 1;
}

int kunlink(char *pathname) {
    if (pathname[0] == '/' && pathname[1] == '\0') {
        // trying to unlink root
        return -1;
    }
    char *last = (char *)malloc(14);
    short super_inode;
    int retp = check_pathname(pathname, last, &super_inode);
    printf("Deleting inode = %d\n", retp);
    if (retp == 0 || retp == -1) {
        // does not exist file or error
        return -1;
    }
    if (retp > 0) {
		if (fd_table[retp] != NULL) {
	        // fd is already open
	        return -1;
	    }
        // File exist, we can strart to remove
        // Get inode number
        int inode = retp;
        if (memcmp(dir, GET_INODE_TYPE(inode), 3) == 0) {
            // Check if it is a DIR file
            if (GET_INODE_SIZE(inode) != 0) {
                // removing non-empty directory.
                return -1; 
            } else {
                // file size = 0
                // 1. Remove dir
                memset(&(rd->ib[inode]), 0, sizeof(struct Inode));			
				delete_dir_entry(super_inode, last);
				INCR_FREEINODE;
                // 2. Go to super_inode and remove inode entry
                return 0;
                
            }
        }
        if (memcmp(reg, GET_INODE_TYPE(inode), 3) == 0) {
            // Check if it is a reg file
            // 1. Get file size
			int reg_size = GET_INODE_SIZE(inode);
            // 2. remove file
			unlink_file(inode);
			memset(&(rd->ib[inode]), 0, sizeof(struct Inode));			
            // 3. Go to super_inode and remove inode entry
			recursive_pathname_size_decr(pathname, reg_size);
			INCR_FREEINODE;
            // 4. Traverse filesystem and minus file_size on all super inodes.
            PRINT_FREEINODE_COUNT;
            PRINT_FREEBLOCK_COUNT;
            return 0;
        }
    }
}

int read_dir_entry(short inode, int read_pos, struct Dir_entry *temp_add) {
    // Read 1 dir_entry
    // Build the inode structure first.
    unsigned char *ist = (unsigned char *)malloc(MAX_FILE_SIZE);
    int size = build_inode_structure(inode, ist);
#ifdef debug
    printf("size = %d\n", size);
#endif
    
    if (size == 0) {
        return 0;
    }
    
    if (read_pos >= (size - 1)) {
        // Read_pos is greater then possible size
        return -1;
    }
    while ((read_pos + 16) <= size) {
        // Read an entry
        struct Dir_entry *d = (struct Dir_entry *)malloc(sizeof(struct Dir_entry));
        memcpy(d, ist + read_pos, 16);
        if (d->inode_number == 0) {
            // An empty Dir_entry
            read_pos += 16;
        } else {
            memcpy(temp_add, d, 16);
            // set the position to the next entry
            fd_table[inode]->read_pos = (read_pos + 16);
            return 1;
        }
    }
    // Nothing read, return EOF
    return 0;
}
int small_itoa(struct Dir_entry *temp_add) {
    // convert inode number into string.
    short num = temp_add->inode_number;
    //printf("num = %d", num);
    int ten = num/10;
    int dig = num%10;
    // 48 = ascii '0'
    *((unsigned char *)temp_add + 14) = (48 + ten);
    *((unsigned char *)temp_add + 15) = (48 + dig);
}

#define USING_ATOI
// using ATOI only to accomdate test script 4
int kreaddir(int fd, char *address) {
    // Again, fd=inode index
    if (fd_table[fd] == NULL) {
        // Not an valid fd
        return -1;
    }
    // Check if it is a DIR file
    if (memcmp(dir, GET_INODE_TYPE(fd), 3) == 0) {
        // read 1 dir_entry from fd
        struct Dir_entry *temp_add = (struct Dir_entry *)malloc(sizeof(struct Dir_entry));
        int ret = read_dir_entry(fd, fd_table[fd]->read_pos, temp_add);
        if (ret == -1) {
            return -1;
        }
        if (ret == 0) {
            // no dir entry
            return 0;
        }
#ifdef USING_ATOI
        small_itoa(temp_add);
#endif
#ifdef debug
        printf("\nret = %d\n", ret);
        printf("%d\n", temp_add->inode_number);
        int zz = 0;
        for (zz; zz < 16; zz++) {
            printf("%d ", *((unsigned char *)temp_add + zz));
        }
        printf("\n");
#endif
        memcpy(address, temp_add, 16);
        
        // Need copy_to_user to finish the copying.
        
        // 1 is success
        return 1;
    } else {
        // Not a Dir file. 
        return -1;
    }
}

/*
int main() {
    init_fs();
    unsigned char *partition_block_location = (rd->bb.byte);
    // printf("Partition block starting point: %d\n", *(char *)partition_block_location);
    // 
    int zz = 0;
    // for (zz; zz < 256; zz++) {
    //     printf("%d ", *((char *)partition_block_location + zz));
    // }

	void *st = (void *)rd->pb;
	printf("this is the pointer: %p\n", rd->pb);
	printf ("this is the pb starting address: %p\n", st);
    
    // printf("\n");
    
    
    // Test write from inode
    //unsigned char file[500] ="0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------0123456789------====";
    //int nnnnnn = set_inode_reg_file(file, 1,  sizeof(file));
    //printf("Size=?%d, Allocated?=%d\n", (int)sizeof(file), nnnnnn);
    //printf("%s\n", GET_FROM_LOCATION(1, 0));
    // Test read from inode
    //int file_size = 500;
    //unsigned char *file_read = (unsigned char *)malloc(file_size);
    //int nnnnnm = get_inode_reg_file(1, file_size, file_read);
    //printf("GET_FILE=\n%s\n", file_read);
    
    

    // for testing
    //int fb = find_free_block();
    //printf("fb = %d\n", fb);
    ///SET_INODE_LOCATION_BLOCK(0, 0, fb);
    //
    
    
    //check_pathname(char *pathname, char* last, short* super_inode)
    
    char *pathname = "/home";
    kmkdir(pathname);
    printf("%d\n", get_inode_index(0, "home"));
	char *another = "/home/yeah";
	kmkdir(another);
    printf("%d\n", get_inode_index(1, "yeah"));
    char *path2 = "/home/test";
	char *path3 = "/home/test2";
    kcreat(path2);
    kcreat(path3);
    printf("%d\n", get_inode_index(1, "test"));


    zz = 256;
    for (zz; zz < 512; zz++) {
        printf("%d ", *((char *)st + zz));
    }
    
    printf("\n");
    
    
    // Get inode block memory location
    void *b0 = &(*rd->ib[1].blocks[0]);
    zz = 0;
    for (zz; zz < 256; zz++) {
        printf("%d ", *((char *)b0 + zz));
    }
    
    printf("\n");
    int p_index = b0 - st;
    
    //unsigned char *ist = (unsigned char *)malloc(MAX_FILE_SIZE);
    //int size = build_inode_structure(0, ist);
    //int ij = 0;
    //printf("size = %d\n", size);
    //for (ij; ij < size; ij++) {
    //    if (ij % 256 == 0) {
    //        printf("\n one block end pos=%d\n", ij);
    //    }
    //    printf("%c ", ist[ij]);
    //}

    
    int i = kopen(path2);
	int k = kopen(path3);
	char *content = "im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuffim writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuffim writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuffim writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff!im writing some stuff,IANENDINGG!@#";
	char *lol = "lolz";
	int ss = strlen(content)+1;
	int kk = strlen(lol)+1;
	kwrite(i, content, ss);
	kwrite(k, lol, kk);
	char *stuff;
	stuff = (char *)malloc(ss);
	printf("this is the size of /home/test: %d\n", GET_INODE_SIZE(i));
	printf("this is the size of /home/test2: %d\n", GET_INODE_SIZE(k));
	printf("this is the size of /home: %d\n", GET_INODE_SIZE(1));
	kread(i, stuff, ss);
	printf("i just read this: %s\n", stuff);
	printf("this is partition index: %d\n",GET_BLOCK_INDEX_PARTITION(GET_INODE_LOCATION_BLOCK(3,0)));
    char *add= (char *)malloc(16);
    kreaddir(0, add);
    
    printf("read dir returned: \n");
    int ij = 0;
    for (ij; ij < 16; ij++) {
        printf("%d ", add[ij]);
    }
    printf("\n");
	free(add);
    //printf("%d\n", (struct Dir_entry *)add.inode_number);
    kclose(i);
	kclose(k);
	kunlink(path2);
	printf("stop here");
#ifdef debug2
    PRINT_FREEINODE_COUNT;
    PRINT_FREEBLOCK_COUNT;

    // test set filename
    char n[16] = "non-root";
    SET_DIR_ENTRY_NAME(0, 0, n);
    //PRINT_DIR_ENTRY_NAME(0, 0);

    // test write to reg file block
    unsigned char x[260] = "testing=testing2=testing3";
    WRITE_TO_FILE(1, x)
    printf("Print direct block write: %s\n", GET_FROM_FILE(1));

    // Assign location in inode (regular file block (0~7))
    ASSIGN_LOCATION(1, 0, 1);
    printf("Print direct block Inode: %s\n", GET_FROM_LOCATION(1, 0));
    
    // Single redirection
    ASSIGN_LOCATION(1, 8, 2);
    ASSIGN_LOCATION_SINGLE_RED(1, 0, 3);
    WRITE_TO_FILE(3, x);
    printf("Print single redir block: %s\n", GET_FROM_LOCATION_SINGLE_RED(1, 0));
    
    // Double redirection
    ASSIGN_LOCATION(1, 9, 4);
    ASSIGN_LOCATION_DOUBLE_FST_RED(1, 0, 5);
    WRITE_TO_FILE(6, x);
    ASSIGN_LOCATION_DOUBLE_SND_RED(1, 0, 0, 6);
    printf("Print double redir block: %s\n", GET_FROM_LOCATION_DOUBLE_RED(1, 0, 0));

    
    // Test bitmap
    int byte=0;
    int block_number =7;
    SET_BITMAP_ALLOCATE_BLOCK(block_number);
    
    PRINT_BITMAP_BIT_STATUS(block_number);
    
    PRINT_BITMAP_BYTE(byte);
    
    SET_BITMAP_FREE_BLOCK(block_number);
    
    PRINT_BITMAP_BIT_STATUS(block_number);
    
    PRINT_BITMAP_BYTE(byte);
#endif    

    // Test freeing a block. directly

}
*/

