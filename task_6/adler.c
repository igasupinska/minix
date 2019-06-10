#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>

#define MOD_ADLER           65521
#define ADLER_SUM_SIZE      8

#define ADLER_BUF_SIZE     512
static u_char adler_buf[ADLER_BUF_SIZE];

/*
* Functions for performing adler sum calculation.
*/
static void restore_initial_state();
static void calc_adler_coeffs(u_char *data, size_t len);
static uint32_t get_adler_sum();

/*
* Function prototypes for the hello driver.
*/
static int adler_open(devminor_t minor, int access, endpoint_t user_endpt);
static int adler_close(devminor_t minor);
static ssize_t adler_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t adler_write(devminor_t minor, u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init(int type, sef_init_info_t *info);
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);

/* Entry points to the hello driver. */
static struct chardriver adler_tab =
{
    .cdr_open	= adler_open,
    .cdr_close	= adler_close,
	.cdr_read	= adler_read,
	.cdr_write  = adler_write,
};

/** State variable to count the number of times the device has been opened.
* Note that this is not the regular type of open counter: it never decreases.
*/
static uint32_t A;
static uint32_t B;
static uint32_t adler_sum;

static int adler_open(devminor_t UNUSED(minor), int UNUSED(access),
    endpoint_t UNUSED(user_endpt))
{
    return OK;
}

static int adler_close(devminor_t UNUSED(minor))
{
    return OK;
}

static ssize_t adler_read(devminor_t UNUSED(minor), u64_t position, 
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
    cdev_id_t UNUSED(id))
{
    u64_t dev_size;
    u_char *ptr;
    int ret;

    /* Additional byte needed for terminating null character. */
    u_char buf[ADLER_SUM_SIZE + 1];

    sprintf(buf, "%08x", get_adler_sum());

    /* This is the total size of our device. */
    dev_size = (u64_t) strlen(buf);

    /* Check for EOF, and possibly limit the read size. */
    if (position >= dev_size) return 0;		/* EOF */
    if (position + size > dev_size)
        size = (size_t)(dev_size - position);	/* limit size */

    /* Copy the requested part to the caller. */
    ptr = buf + (size_t)position;
    if ((ret = sys_safecopyto(endpt, grant, 0, (vir_bytes) ptr, size)) != OK)
        return ret;

    if (size < ADLER_SUM_SIZE)
        return EINVAL;

    /* Reset state of the device after successful read. */
    restore_initial_state();

    /* Return the number of bytes read. */
    return size;
}

static ssize_t adler_write(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
/* Write to one of the driver's minor devices. */
  size_t offset, chunk;
  int ret;

  /* Read input in chunks, recalculate adler sum accordingly. */
  for (offset = 0; offset < size; offset += chunk) {
	chunk = MIN(size - offset, ADLER_BUF_SIZE);
	ret = sys_safecopyfrom(endpt, grant, offset, (vir_bytes)adler_buf,
		chunk);

    calc_adler_coeffs(adler_buf, chunk);

	if (ret != OK) {
		printf("random: sys_safecopyfrom failed for proc %d,"
			" grant %d\n", endpt, grant);
		return ret;
	}
  }

  return size;
}

static int sef_cb_lu_state_save(int UNUSED(state)) {
    /* Save adler sum coefficients. */
    ds_publish_u32("A", A, DSF_OVERWRITE);
    ds_publish_u32("B", B, DSF_OVERWRITE);

    return OK;
}

static int lu_state_restore() {
    /* Restore adler sum coefficients. */
    u32_t restored_A, restored_B;
    ds_retrieve_u32("A", &restored_A);
    ds_retrieve_u32("B", &restored_B);
    ds_delete_u32("A");
    ds_delete_u32("B");
    A = (uint32_t) restored_A;
    B = (uint32_t) restored_B;

    return OK;
}

static void sef_local_startup()
{
	/*
     * Register init callbacks. Use the same function for all event types
     */
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_lu(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);

    /*
     * Register live update callbacks.
     */
    /* - Agree to update immediately when LU is requested in a valid state. */
    sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
    /* - Support live update starting from any standard state. */
    sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
    /* - Register a custom routine to save the state. */
    sef_setcb_lu_state_save(sef_cb_lu_state_save);

    /* Let SEF perform startup. */
    sef_startup();
}

static int sef_cb_init(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the adler driver. */
	int do_announce_driver = TRUE;

    switch(type) {
	   case SEF_INIT_FRESH:
            restore_initial_state();
        break;

        case SEF_INIT_LU:
            /* Restore the state. */
            lu_state_restore();
            do_announce_driver = FALSE;
        break;

        case SEF_INIT_RESTART:
            restore_initial_state();
        break;
    }

    /* Announce we are up when necessary. */
    if (do_announce_driver) {
        chardriver_announce();
    }

    /* Initialization completed successfully. */
    return OK;
}

static uint32_t get_adler_sum() {
    return (B << 16) | A;

}

/* Adapted from: https://pl.wikipedia.org/wiki/Adler-32 */
static void calc_adler_coeffs(u_char *data, size_t len) {

    while (len) {
            unsigned tlen = len > 5550 ? 5550 : len;
            len -= tlen;
            do {
                    A += *data++;
                    B += A;
            } while (--tlen);
            A = (A & 0xffff) + (A >> 16) * (65536-MOD_ADLER);
            B = (B & 0xffff) + (B >> 16) * (65536-MOD_ADLER);
    }
    /* It can be shown that a <= 0x1013a here, so a single subtract will do. */
    if (A >= MOD_ADLER)
            A -= MOD_ADLER;
    /* It can be shown that b can reach 0xffef1 here. */
    B = (B & 0xffff) + (B >> 16) * (65536-MOD_ADLER);
    if (B >= MOD_ADLER)
            B -= MOD_ADLER;
}

static void restore_initial_state() {
    A = 1;
    B = 0;
}

int main(void) {
	/*
	* Perform initialization.
	*/
	sef_local_startup();
	/*
	* Run the main loop.
	*/
	chardriver_task(&adler_tab);
	return OK;
}