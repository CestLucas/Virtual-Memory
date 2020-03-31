// Include files
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define  N_OBJS_PER_SLAB  64
#define  MIN_SIZE_ALLOC 1024
#define  MEM_SIZE 1024 * 1024
#define  OFFSET 4

// Self-defined memory data structure
// reference: https://www.geeksforgeeks.org/queue-set-2-linked-list-implementation/
// reference: https://www.geeksforgeeks.org/priority-queue-using-linked-list/
typedef struct mem_chunk{
	int start_loc;
	int chunk_size;
	int used;
	void *val_ptr;
	struct mem_chunk *next_chunk;
} Chunk;

// struct to keep track of mem buddies
typedef struct buddy{
	int b1_loc;
	int b2_loc;
	unsigned int active;
	struct buddy *next_pair;
} Buddies;

// struct for slab
typedef struct slab{
	void * ptr; // point to the beginning of the slab block
	int chunk_size; // use this to identify and group chunks
	int allocated; // number of chunks in the slab section
	struct slab *next_slab;
} Slab;

// Functional prototypes
void setup( int malloc_type, int mem_size, void* start_of_memory );
void *my_malloc( int size );
void my_free( void *ptr );

// helper functions
void *buddy_system( int size );
void *slab_alloc( int size );
int next_power_of_2 ( int num );
void *find_hole( int alloc_size );
void buddy_free( void *ptr );
void slab_free( void *ptr );

// memory struct interfaces
Chunk *create_chunk(int loc, int size, int used, void *ptr);
void new_chunk(Chunk** first, int loc, int size, int used, void *ptr);
Chunk *find_chunk(Chunk** first, int loc); 
Chunk *find_chunk_by_ptr(Chunk** first, void *ptr); 
Chunk *mem_free(Chunk** first, void *ptr);
void remove_chunk(Chunk** first_chunk, int start_loc);
void free_slab_chunk(Chunk** first, void *ptr);

// buddy struct interfaces
Buddies *create_buddies(int b1_loc, int b2_loc, int active);
void new_buddies(Buddies** first_pair, int b1_loc, int b2_loc, int active);

// slab struct interfaces
Slab *create_slab(void * ptr, int chunk_size, int allocated);
void new_slab(Slab** first_slab, void * ptr, int chunk_size, int allocated);
Slab *find_slab(Slab** first_slab, int chunk_size); 
void remove_slab(Slab** first_slab, void *ptr);

// varibales for setup
unsigned int type; //0 - buddy system; 1 - slab alloc
static int size;
void *start_ptr;
unsigned int is_start = 1;
Chunk *first_chunk = NULL;
Buddies *first_pair = NULL;
Slab *first_slab = NULL;

////////////////////////////////////////////////////////////////////////////
//
// Function     : setup
// Description  : initialize the memory allocation system
//
// Inputs       : malloc_type - the type of memory allocation method to be used [0..3] where
//                (0) Buddy System
//                (1) Slab Allocation

void setup( int malloc_type, int mem_size, void* start_of_memory ){
	type = malloc_type;
	size = mem_size;
	start_ptr = start_of_memory;
}

////////////////////////////////////////////////////////////////////////////
//
// Function     : my_malloc
// Description  : allocates memory segment using specified allocation algorithm
//
// Inputs       : size - size in bytes of the memory to be allocated
// Outputs      : -1 - if request cannot be made with the maximum mem_size requirement

void *my_malloc( int size ){
	if (type == 0) {
		return buddy_system(size);
	}
	else if (type == 1) {
		return slab_alloc(size);
	}
}

void *buddy_system( int size ){
	if (is_start == 1){
		is_start = 0;
		if (size <= MEM_SIZE){
			// creating partition map
			int p_size = next_power_of_2(size + OFFSET);
			int p_loc = 0;
			if (p_size < MIN_SIZE_ALLOC) {
				p_size = MIN_SIZE_ALLOC;
			}
		
			first_chunk = create_chunk(p_loc, p_size, 1, start_ptr); // 1 if the block is used, 0 if not used

			p_loc += p_size;
			while (p_loc + p_size <= MEM_SIZE){
				new_chunk(&first_chunk, p_loc, p_size, 0, start_ptr + p_loc);
				p_loc = p_loc + p_size;
				p_size *= 2;
			}
			
			// add the end of memory to the end of the queue just for convinience
			new_chunk(&first_chunk, MEM_SIZE, 0, 0, NULL);
			
			// now update the buddies record
			if (first_chunk->chunk_size <= MEM_SIZE / 2) { // we guarentee that the first chunk has a buddy
				first_pair = create_buddies(first_chunk->start_loc, first_chunk->next_chunk->start_loc, 1);
				
				Chunk* tmp = first_chunk->next_chunk->next_chunk;
				while (tmp!= NULL && tmp->start_loc != MEM_SIZE){
					new_buddies(&first_pair, first_chunk->start_loc, tmp->start_loc, 1);
					tmp = tmp->next_chunk;
				}
			}
			
			return start_ptr + OFFSET;
		}
	}
	else{
		int allocation_size = next_power_of_2(size + OFFSET);
		
		if (allocation_size < MIN_SIZE_ALLOC) {
			allocation_size = MIN_SIZE_ALLOC;
		}
	
		void * alloc_pos = find_hole(allocation_size);
		if ((intptr_t) alloc_pos == -1)
			return alloc_pos;
		else
			return alloc_pos + OFFSET;
	}
}

void *find_hole(int alloc_size) { //if a hole is found, return allocated start location
	// check if allocation size is even bigger than the actual memory size
	if (alloc_size > MEM_SIZE){
		return (void *) -1;
	}
	
	// first try to find a hole of exact same requested allocation size
	Chunk * first = first_chunk;
	while (first != NULL){
		if (first->chunk_size == alloc_size && first->used == 0){
			first->used = 1;
			void * new_ptr = start_ptr + first->start_loc;
			first->val_ptr = new_ptr;
			return new_ptr;
		}
		first = first->next_chunk;
	}

	Chunk *tmp = first_chunk; // looking from the first memory block
	while (tmp != NULL){
		int chunk_loc = tmp->start_loc;
		int chunk_size = tmp->chunk_size;

		if (tmp->start_loc == MEM_SIZE){
			// mem cannot fit in a hole, return -1
			return (void *) -1;
		}
		else if (alloc_size > chunk_size || tmp->used == 1){
			tmp = tmp->next_chunk;
		}
		else { // need to split some mem blocks
			int next_loc = tmp->next_chunk->start_loc;
			// update partition map
			int p_size = alloc_size;
			int p_loc = chunk_loc;
			tmp->chunk_size = p_size;
			tmp->used = 1;
			
			// update the allocated mem loc
			void * new_ptr = start_ptr + chunk_loc;
			tmp->val_ptr = new_ptr;

			p_loc += p_size;
			// update buddy record
			new_buddies(&first_pair, chunk_loc, p_loc, 1);
			// create buddy in the memory
			new_chunk(&first_chunk, p_loc, p_size, 0, start_ptr + p_loc);
			
			// now keep partitioning the memory
			p_loc += p_size;
			p_size *= 2;
			while (p_loc + p_size <= next_loc){
				new_buddies(&first_pair, chunk_loc, p_loc, 1);
				new_chunk(&first_chunk, p_loc, p_size, 0, start_ptr + p_loc);
				p_loc += p_size;
				p_size *= 2;
			}

			return new_ptr;
		}
	}
}

void *slab_alloc( int size ){
	// check if max allocatable exceeds memory size
	int alloc_size = next_power_of_2( (size + OFFSET) * N_OBJS_PER_SLAB);
	if (alloc_size > MEM_SIZE) {
		return (void *) -1;
	}

	if (is_start == 1){
		// initialize the memory using buddy
		void * ptr = buddy_system( alloc_size - OFFSET );
		
		// initialize the slab list
		first_slab = create_slab(start_ptr, size + OFFSET, 1);

		// add a dummy slab just to prevent over-deletion
		new_slab(&first_slab, NULL, 0, 0);

		// allocate N_OBJS_PER_SLAB of slab spots in the mem chunk
		new_chunk(&first_chunk, 2 * OFFSET, size + OFFSET, 1, start_ptr + 2 * OFFSET); // allocate the first chunk

		unsigned int allocated = 1;
		while (allocated < N_OBJS_PER_SLAB){
			int start_loc = 2 * OFFSET + allocated * (size + OFFSET);
			new_chunk(&first_chunk, start_loc, size + OFFSET, 0, start_ptr + start_loc);
			allocated++;
		}

		return ptr + OFFSET;
	}
	else{
		// try finding existing slab record for the chunk type
		Slab * rec = find_slab(&first_slab, size + OFFSET);
		
		if ((intptr_t) rec != -1){ //found a fillable slab
			// loop through the slab, find a spot to allocate the chunk to the memory
			Chunk *tmp = find_chunk_by_ptr(&first_chunk, rec->ptr);

			int start_loc = tmp->start_loc + 2 * OFFSET;
			int check_range = start_loc + N_OBJS_PER_SLAB * rec->chunk_size;

			tmp = tmp->next_chunk;

			while (tmp->start_loc < check_range && tmp->used == 1){
				tmp = tmp->next_chunk;
			}
			
			tmp->used = 1;
			rec->allocated += 1;

			return tmp->val_ptr;
		}
		else { // 1. new chunk type is arriving 2. slab found is full
			// use buddy system to find a hole to fit the new slab
			void * ptr = buddy_system( alloc_size - OFFSET);
			if ((intptr_t) ptr == -1)
				return ptr;
			
			// create and link new slab table
			//new_slab(Slab** first_slab, void * ptr, int chunk_size, int allocated)
			new_slab(&first_slab, ptr - OFFSET, size + OFFSET, 1);

			Chunk* q = first_chunk;

			// add the first mem chunk to the memory
			Chunk * a_chunk = find_chunk_by_ptr(&first_chunk, ptr - OFFSET);
			int start_loc = a_chunk->start_loc + 2 * OFFSET;
			new_chunk(&first_chunk, start_loc, size + OFFSET, 1, ptr + OFFSET);

			unsigned int allocated = 1;
			while (allocated < N_OBJS_PER_SLAB){
				start_loc += (size + OFFSET);
				new_chunk(&first_chunk, start_loc, size + OFFSET, 0, start_ptr + start_loc);
				allocated++;
			}

			return ptr + OFFSET;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Utility function
int next_power_of_2 ( int num ){
	// perform bitwise ops (assume maximum 32 size no greater than 32 bit)
	num -= 1;
	num |= num >> 1;
	num |= num >> 2;
	num |= num >> 4;
	num |= num >> 8;
	num |= num >> 16;
	return num+1;
}

////////////////////////////////////////////////////////////////////////////
//
// Function     : my_free
// Description  : deallocated the memory segment being passed by the pointer
//
// Inputs       : ptr - pointer to the memory segment to be free'd
// Outputs      :

void my_free( void *ptr )
{
	if (type == 0) {
		buddy_free(ptr);
	}
	else if (type == 1) {
		slab_free(ptr);
	}
}

void buddy_free( void *ptr )
{
	// first mark the memory block as unused
	Chunk* freed_loc = mem_free(&first_chunk, ptr - OFFSET);
	int loc = freed_loc->start_loc;

	//perform defragmentation
	Buddies *tmp = first_pair;
	while (tmp != NULL && tmp->b1_loc <= loc){ //loop through the buddies list to find legid pairs
		if (tmp->active == 1){
			int check_range = tmp->b1_loc + (tmp->b2_loc - tmp->b1_loc) * 2;
			unsigned int can_merge = 1;
			Chunk *b1 = find_chunk(&first_chunk, tmp->b1_loc);
			if ((intptr_t) b1 == -1){
				tmp->active = 0;
			}
			else {
				Chunk *iter = b1;
				while (iter != NULL && iter->start_loc < check_range && can_merge == 1){
					if (iter->used == 1){
						can_merge = 0;
					}
					iter = iter->next_chunk;
				}
				if (can_merge == 1){
					Chunk *rm = b1->next_chunk;
					// remove intermediate chunks
					while (rm != NULL && rm->start_loc < check_range){
						int rm_loc = rm->start_loc;
						rm = rm->next_chunk;
						remove_chunk(&first_chunk, rm_loc);
					}
					// extend first chunk size
					b1->chunk_size = check_range;
				
					// deactivate relevant buddy record
					tmp->active = 0;
				}
			}
		}

		tmp = tmp->next_pair;
	}
}

void slab_free( void *ptr )
{
	Slab *rm = first_slab;

	unsigned int slab_found = 0;
	void * ptr_range = rm->ptr + 2 * OFFSET + rm->chunk_size * N_OBJS_PER_SLAB;
	// check if the first slab is the one
	if (ptr > rm->ptr && ptr < ptr_range)
		slab_found = 1;
	
	// if not in the first one, loop through the whole 
	if (slab_found == 0)
		rm = rm->next_slab;
	while (rm != NULL && slab_found == 0){
		ptr_range = rm->ptr + 2 * OFFSET + rm->chunk_size * N_OBJS_PER_SLAB;
		if (!(ptr > rm->ptr && ptr < ptr_range))
			rm = rm->next_slab;
		else
			slab_found = 1;
	}
	
	if (slab_found == 0)
		return;
	
	// found the corresponding slab. Now need to consider some cases
	if (rm->allocated > 1){ // if more than 1 mem chunks in the slab
		// 1. delete the chunk
		free_slab_chunk(&first_chunk, ptr);
		// 2. update slab table
		rm->allocated -=1;
	}
	else if (rm->allocated == 1){
		// just need to free the mem block and delete the slab record
		// delete all pre-allocated slab slots in the mem range
		Chunk* remover = find_chunk_by_ptr(&first_chunk, rm->ptr);
		
		int remove_start = remover->start_loc + 2 * OFFSET;
		unsigned removed = 0;

		while (removed < N_OBJS_PER_SLAB){
			remove_chunk(&first_chunk, remove_start);
			remove_start += rm->chunk_size;
			removed++;

		}

		buddy_free( rm->ptr + OFFSET);

		remove_slab(&first_slab, rm->ptr);
	}
	else
		return;
	
}

////////////////////////////////////////////////////////////////////////////
// Memory data structure implementations
Chunk *create_chunk(int loc, int size, int used, void *ptr){
    Chunk* tmp = (Chunk*)malloc(sizeof(Chunk));
    tmp->start_loc = loc;
    tmp->chunk_size = size;
    tmp->used = used;
    tmp->val_ptr = ptr;
    tmp->next_chunk = NULL;
    
    return tmp;
}

void new_chunk(Chunk** first, int loc, int size, int used, void *ptr){
    Chunk* start = (*first);

    Chunk* tmp = create_chunk(loc, size, used, ptr);
    
    if ((*first)->start_loc > loc) {
        
        tmp->next_chunk = *first;
        (*first) = tmp;
    }
    else {
        while (start->next_chunk != NULL &&
               start->next_chunk->start_loc < loc) {
            start = start->next_chunk;
        }
        
        tmp->next_chunk = start->next_chunk;
        start->next_chunk = tmp;
    }
}

Chunk *find_chunk(Chunk** first, int loc){
    Chunk* tmp = (*first);
    
    if (tmp != NULL && tmp->start_loc == loc) {
        return tmp;
    }
    else {
        while (tmp != NULL && tmp->start_loc != loc){
            tmp = tmp->next_chunk;
        }
	//found
        if(tmp != NULL && tmp->start_loc != MEM_SIZE){
	    return tmp;
        }
	// not found
	return (void *) -1;
    }
}

Chunk *find_chunk_by_ptr(Chunk** first, void *ptr){
    Chunk* tmp = (*first);
    
    if (tmp != NULL && tmp->val_ptr == ptr) {
        return tmp;
    }
    else {
        while (tmp != NULL && tmp->val_ptr != ptr){
            tmp = tmp->next_chunk;
        }
	//found
        if(tmp != NULL && tmp->start_loc != MEM_SIZE){
	    return tmp;
        }
	// not found
	return (void *) -1;
    }
}

Chunk *mem_free(Chunk** first, void *ptr){
    Chunk* tmp = (*first);
    
    if (tmp != NULL && tmp->val_ptr == ptr){
        tmp->used = 0;
	return tmp;
    }
    else{
        while (tmp != NULL && tmp->val_ptr != ptr){
            tmp = tmp->next_chunk;
        }
	//found
        if(tmp != NULL && tmp->start_loc != MEM_SIZE){
            tmp->used = 0;
	    return tmp;
        }
    }

}

void free_slab_chunk(Chunk** first_chunk, void *ptr){
    Chunk* tmp = (*first_chunk);
    
    if (tmp != NULL && tmp->val_ptr == ptr){
        tmp->used = 0;
    }
    else{
        while (tmp != NULL && tmp->val_ptr != ptr){
            tmp = tmp->next_chunk;
        }
        if(tmp != NULL && tmp->start_loc != MEM_SIZE){
            tmp->used = 0;
        }
    }
}

void remove_chunk(Chunk** first_chunk, int start_loc){
    Chunk* tmp = (*first_chunk);
    Chunk* prev = tmp;
    
    if (tmp != NULL && tmp->start_loc == start_loc){
        (*first_chunk) = tmp->next_chunk;
        free(tmp);
    }
    else{
        while (tmp != NULL && tmp->start_loc != start_loc){
            prev = tmp;
            tmp = tmp->next_chunk;
        }
        if(tmp != NULL && tmp->start_loc != MEM_SIZE){
            prev->next_chunk = tmp ->next_chunk;
            free(tmp);
        }
    }
}


////////////////////////////////////////////////////////////////////////////
// Buddies data structure implementations
Buddies *create_buddies(int b1_loc, int b2_loc, int active){
    Buddies* tmp = (Buddies*)malloc(sizeof(Buddies));
    tmp->b1_loc = b1_loc;
    tmp->b2_loc = b2_loc;
    tmp->active = active;
    tmp->next_pair = NULL;
    
    return tmp;
}

void new_buddies(Buddies** first_pair, int b1_loc, int b2_loc, int active){
    Buddies* start = (*first_pair);

    Buddies* tmp = create_buddies(b1_loc, b2_loc, active);
    
    if ((*first_pair)->b1_loc > b1_loc) {  //b1_loc is the key for this data structure
        
        tmp->next_pair = *first_pair;
        (*first_pair) = tmp;
    }
    else {
        while (start->next_pair != NULL &&
               start->next_pair->b1_loc <= b1_loc) {
            start = start->next_pair;
        }
        
        tmp->next_pair = start->next_pair;
        start->next_pair = tmp;
    }
}

////////////////////////////////////////////////////////////////////////////
// Slab data structure implementations
// slab struct interfaces

Slab *create_slab(void * ptr, int chunk_size, int allocated){
    Slab* tmp = (Slab*)malloc(sizeof(Slab));
    tmp->ptr = ptr;
    tmp->chunk_size = chunk_size;
    tmp->allocated = allocated;
    tmp->next_slab = NULL;
 
    return tmp;
}

void new_slab(Slab** first_slab, void * ptr, int chunk_size, int allocated){
    Slab* start = (*first_slab);

    Slab* tmp = create_slab(ptr, chunk_size, allocated);
    
    if ((*first_slab)->chunk_size > chunk_size) {  //b1_loc is the key for this data structure
        
        tmp->next_slab = *first_slab;
        (*first_slab) = tmp;
    }
    else {
        while (start->next_slab != NULL &&
               start->next_slab->chunk_size <= chunk_size) {
            start = start->next_slab;
        }
        
        tmp->next_slab = start->next_slab;
        start->next_slab = tmp;
    }
}

Slab *find_slab(Slab** first_slab, int chunk_size) {  //try to find an empty slab
    Slab* tmp = (*first_slab);
    
    if (tmp != NULL && tmp->chunk_size == chunk_size && tmp->allocated < N_OBJS_PER_SLAB) {
        return tmp;
    }
    else {
        while (tmp != NULL && (tmp->chunk_size != chunk_size || tmp->allocated == N_OBJS_PER_SLAB)){
                tmp = tmp->next_slab;
        }
	//found
        if(tmp != NULL){
	    return tmp;
        }
	// not found
	return (void *) -1;
    }
}

void remove_slab(Slab** first_slab, void *ptr) {
    Slab* tmp = (*first_slab);
    Slab* prev = tmp;
    
    if (tmp != NULL && tmp->ptr == ptr){
	(*first_slab) = tmp->next_slab;
        free(tmp);
    }
    else{
        while (tmp != NULL && tmp->ptr != ptr){
            prev = tmp;
            tmp = tmp->next_slab;
        }
        if(tmp != NULL){
            prev->next_slab = tmp ->next_slab;
            free(tmp);
        }
    }
}





