#include "cgroup.h"
#include "defs.h"
#include "cgfs.h"

#define MAX_DES_DEF "64"
#define MAX_DEP_DEF "64"
#define MAX_CGROUP_FILE_NAME_LENGTH 64

struct cgroup cgroups[NPROC];

static int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}


/*Raise the value of the number located in the string num by 1.*/
static void
increment_num_string(char *num)
{
	int i;
	for(i = 0; num[i + 1] != '\0'; i++)
		;
	if(num[i] == '9')
		for(;i >= 0 && num[i] == '9'; i--)
			num[i] = '0';
	if(i >= 0)
        num[i]++;
	if(num[0] == '0'){
		for(i = 0; num[i] != '\0'; i++)
			;
		num[i] = '0';
		num[i+1] = '\0';
		num[0] = '1';
	}
}

/*Lower the value of the number located in the string num by 1.*/
static void
decrement_num_string(char *num)
{
	if(*num == '0')
		return;
	int i;
	for(i = 0; num[i + 1] != '\0'; i++)
		;
	if(num[i] == '0')
		for(;i >= 0 && num[i] == '0'; i--)
			num[i] = '9';
	num[i]--;
	if(num[0] == '0'){
		for(i = 0; num[i] == '0'; i++)
			;
		if(num[i] != '\0'){
			for(int j = 0; num[i] != '\0'; j++){
				num[j] = num[i];
				i++;
			}
			num[i - 1] = '\0';
		}
	}
}

static int 
atoi(char* str) 
{ 
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) 
        res = res * 10 + str[i] - '0'; 

    return res; 
} 

static void
initialize_cgroup_depth(struct cgroup * cgroup){
	char *depth = cgroup->depth;
	char *parentdepth = cgroup->parent->depth;
	/*Copy parents depth into the cgroup's depth field.*/
	if(*parentdepth != 0)
		for(int i = 0; i < MAX_DEPTH_MAX_SIZE && (*depth++ = *parentdepth++) != 0; i++)
			;
	/*Increment the copied depth by 1.*/	
	increment_num_string(cgroup->depth);
}




struct cgroup * cgroup_root(void)
{
    return &cgroups[0];
}

struct cgroup * cgroup_create(char* path)
{
	char parent_path[MAX_PATH_LENGTH];
	char new_dir_name[MAX_PATH_LENGTH];
	
	get_cg_file_dir_path_and_file_name(path, parent_path, new_dir_name);
		
	struct cgroup *parent_cgp = get_cgroup_by_path(parent_path);
	/*Cgroup has to be created as a child of another cgroup. (Root cgroup is not created here)*/
	if(parent_cgp == 0)
		return 0;
	
	/*Check if we are allowed to create a new cgorup at the path. For each ancestor check that we haven't reached maximum number of descendants or maximum subtree depth.*/
	struct cgroup *parent_cgp_temp = parent_cgp;
	for(int i = 0; parent_cgp_temp != 0; i++){
		if(atoi(parent_cgp_temp->max_depth_value) <= i)
			panic("cgroup_create: max depth allowed reached");
		
		if(strcmp(parent_cgp_temp->max_descendants_value, parent_cgp_temp->nr_descendants) == 0)
			panic("cgroup_create: max number of descendants allowed reached");
		
		parent_cgp_temp = parent_cgp_temp->parent;
	}
	
	/*Find avalible cgroup slot.*/
	struct cgroup *new_cgp = 0;
	for(int i = 1; i < sizeof(cgroups) / sizeof(cgroups[0]); i++)
		if(*(cgroups[i].cgroup_dir_path) == 0){
			new_cgp = &cgroups[i];
			break;
		}
	
	/*Check if we have found an avalible slot.*/
	if(new_cgp == 0)
		panic("cgroup_create: no avalible cgroup slots");
	
	/*Initialize the new cgroup.*/
	cgroup_initialize(new_cgp, path, parent_cgp);
	
	/*Update number of descendant cgroups for each ancestor.*/
	while(parent_cgp != 0){
		increment_num_string(parent_cgp->nr_descendants);
		parent_cgp = parent_cgp->parent;
	}
	
	return new_cgp;
}

int cgroup_delete(char* path)
{
	/*Get cgroup at given path.*/
	struct cgroup *cgp = get_cgroup_by_path(path);
	/*If no cgroup at given path return -1.*/
	if(cgp == 0)
		return -1;
	
	/*Check if we are allowed to delete the cgroup. Check if the cgroup has descendants or processes in it.*/
	if(*(cgp->nr_descendants) != '0' || (cgp->num_of_procs && cgp != cgroup_root()))
		return -2;
		
	/*Delete the path.*/	
	*(cgp->cgroup_dir_path) = '\0';
	
	/*Update number of descendant cgroups for each ancestor.*/
	cgp = cgp->parent;
	while(cgp != 0){
		decrement_num_string(cgp->nr_descendants);
		cgp = cgp->parent;
	}

	return 0;
}

void cgroup_initialize(struct cgroup * cgroup, char* path, struct cgroup * parent_cgroup)
{
	/*Check if the cgroup is the root or not and initialize accordingly.*/
	if(parent_cgroup == 0){
		cgroup->cpu_controller_avalible = 1;
		cgroup->cpu_controller_enabled = 1;
		*(cgroup->depth) = '0';	
		*(cgroup->cgroup_dir_path) = 0;
	}else{
		cgroup->parent = parent_cgroup;
		
		/*Cgroup's cpu controller avalible only when it is enabled in the parent.*/
		if(parent_cgroup->cpu_controller_enabled)
			cgroup->cpu_controller_avalible = 1;
		else
			cgroup->cpu_controller_avalible = 0;
		
		cgroup->cpu_controller_enabled = 0;
		initialize_cgroup_depth(cgroup);
		set_cgroup_dir_path(cgroup, path);
	}
	
	cgroup->num_of_procs = 0;
	cgroup->populated = 0;
	set_max_descendants_value(cgroup, MAX_DES_DEF);
	set_max_depth_value(cgroup, MAX_DEP_DEF);
	set_nr_descendants(cgroup, "0");
	set_nr_dying_descendants(cgroup, "0");	

	cgroup->cpu_account_frame = 0;
    cgroup->cpu_percent = 0;
    cgroup->cpu_time = 0;
    cgroup->cpu_time_limit = ~0;
    cgroup->cpu_account_period = 1 * 100 * 1000;
}

int cgroup_insert(struct cgroup * cgroup, struct proc * proc)
{
    // Whether a free slot was found.
    int found = 0;

    // Index of the free slot.
    unsigned int index = 0;

    // Find available slot.
    for (unsigned int i = 0; i < sizeof(cgroup->proc) / sizeof(*cgroup->proc); ++i) {
        // If not found yet, and the current entry is available, save the
        // index and indicate that an entry was found.
        if (!found && !cgroup->proc[i]) {
            index = i;
            found = 1;
            continue;
        }

        // If process was found, return success.
        if (proc == cgroup->proc[i]) {
            return 0;
        }
    }

    // If not found, return a failure.
    if (!found) {
        return -1;
    }

    // Erase the proc from the other cgroup.
    if (proc->cgroup) {
        cgroup_erase(proc->cgroup, proc);
    }

    // Associate the process with the cgroup.
    cgroup->proc[index] = proc;

    // Set the cgroup of the current process.
    proc->cgroup = cgroup;
	
	// Update current number of processes in cgroup subtree for all ancestors.
	while(cgroup != 0){
		cgroup->num_of_procs++;
		cgroup->populated = 1;
		cgroup = cgroup->parent;
	}

    return 0;
}

void cgroup_erase(struct cgroup * cgroup, struct proc * proc)
{
    // Iterate all cgroup processes.
    for (unsigned int i = 0; i < sizeof(cgroup->proc) / sizeof(*cgroup->proc); ++i) {
        // If process was found, remove it from the cgroup.
        if (proc == cgroup->proc[i]) {
            proc->cgroup = 0;
            cgroup->proc[i] = 0;
			
			// Update current number of processes in cgroup subtree for all ancestors.
			while(cgroup != 0){
				cgroup->num_of_procs--;
				if(cgroup->num_of_procs == 0)
					cgroup->populated = 0;
				cgroup = cgroup->parent;
			}
			break;
		}
    }
}

int enable_cpu_controller(struct cgroup * cgroup)
{
	// If cgroup has processes in it, controllers can't be enabled.
	if(!cgroup || cgroup->populated == 1)
		return -1;
	
	// If controller is enabled do nothing.
	if(cgroup->cpu_controller_enabled)
		return 0;
	
	if(cgroup->cpu_controller_avalible){
		
		/* TODO: complete activation of controller. */
		
		// Set cpu controller to enabled.
		cgroup->cpu_controller_enabled = 1;
		// Set cpu controller to avalible in all child cgroups.
		for(int i = 1; i < sizeof(cgroups) / sizeof(cgroups[0]); i++)
			if(cgroups[i].parent == cgroup)
				cgroups[i].cpu_controller_avalible = 1;
	}
	
	return 0;
}

int disable_cpu_controller(struct cgroup * cgroup)
{
	if(!cgroup)
		return -1;
	
	// If controller is disabled do nothing.
	if(!cgroup->cpu_controller_enabled)
		return 0;
	
	// Check that all child cgroups have cpu controller disabled. (cannot disable controller when children have it enabled)
	for(int i = 1; i < sizeof(cgroups) / sizeof(cgroups[0]); i++)
		if(cgroups[i].parent == cgroup && cgroups[i].cpu_controller_enabled)
			return -1;
	
	/* TODO: complete deactivation of controller. */
	
	// Set cpu controller to enabled.
	cgroup->cpu_controller_enabled = 0;
	// Set cpu controller to unavalible in all child cgroups.
	for(int i = 1; i < sizeof(cgroups) / sizeof(cgroups[0]); i++)
		if(cgroups[i].parent == cgroup)
			cgroups[i].cpu_controller_avalible = 0;
	
	return 0;
}

void set_cgroup_dir_path(struct cgroup * cgroup, char* path)
{	
	char *cgroup_dir_path = cgroup->cgroup_dir_path;
	if(*path != 0)
		for(int i = 0; (i < sizeof(cgroup->cgroup_dir_path)) && ((*cgroup_dir_path++ = *path++) != 0); i++)
			;
}

struct cgroup * get_cgroup_by_path(char* path)
{
	if(*path != 0)
		for(int i = 0; i < sizeof(cgroups) / sizeof(cgroups[0]); i++)
			if(strcmp(cgroups[i].cgroup_dir_path, path) == 0)
				return &cgroups[i];
	
	return 0;
}

void set_max_descendants_value(struct cgroup * cgroup, char* value)
{
	char *max_descendants_value = cgroup->max_descendants_value;
	if(*value != 0)
		for(int i = 0; i < MAX_DECS_MAX_SIZE && (*max_descendants_value++ = *value++) != 0; i++)
			;
}

void set_max_depth_value(struct cgroup * cgroup, char* value)
{
	char *max_depth_value = cgroup->max_depth_value;
	if(*value != 0)
		for(int i = 0; i < MAX_DEPTH_MAX_SIZE && (*max_depth_value++ = *value++) != 0; i++)
			;
}

void set_nr_descendants(struct cgroup * cgroup, char* value)
{
	char *nr_descendants = cgroup->nr_descendants;
	if(*value != 0)
		for(int i = 0; i < NR_DESC_MAX_SIZE && (*nr_descendants++ = *value++) != 0; i++)
			;
}

void set_nr_dying_descendants(struct cgroup * cgroup, char* value)
{
	char *nr_dying_descendants = cgroup->nr_dying_descendants;
	if(*value != 0)
		for(int i = 0; i < NR_DYING_DESC_MAX_SIZE && (*nr_dying_descendants++ = *value++) != 0; i++)
			;
}

void get_cgroup_names_at_path(char *buf, char *path)
{
	if(*path == 0)
		return;	
	
	for(int i = 1; i < sizeof(cgroups) / sizeof(cgroups[0]); i++)
		if(strcmp(cgroups[i].parent->cgroup_dir_path, path) == 0){
			char *child_name = &(cgroups[i].cgroup_dir_path[strlen(path) + 1]);
			int child_name_len = strlen(child_name);
			while(*child_name != 0)
				*buf++ = *child_name++;
			buf += MAX_CGROUP_FILE_NAME_LENGTH - child_name_len;
		}
}

