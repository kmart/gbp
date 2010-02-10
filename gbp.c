#include <stdlib.h>

#include <ruby.h>
#include <gbp/TL.h>

VALUE mChipcard;                /* Chipcard module */
VALUE mGBP;                     /* PCSC module */

VALUE cReader;                  /* Reader class */

#define GBP_BUFSIZE     256     /* max bufsize for output */
#define STD_TIMEOUT     5000    /* default 5000 ms timeout value */

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

typedef struct _gbp_com {
        char *tty;              /* tty device  */
        unsigned char ncom;     /* communication number (1 to 4)  */
        unsigned long vcom;     /* communication speed (9600 to 38400 */
        unsigned long timeout;  /* timeout in ms */
        int in_use;
        unsigned char ibuf[GBP_BUFSIZE];        /* input buffer */
        unsigned char ibuf_len;
        unsigned char obuf[GBP_BUFSIZE];        /* output buffer */
        unsigned char obuf_len;
} gbp_com;

gbp_com *gbp_new()
{
        gbp_com *p;

        p = (gbp_com *) malloc(sizeof(gbp_com));
        if (p == NULL) {
        }

        p->tty = (char *) strdup("/dev/ttyS0");
        p->ncom = 1;
        p->vcom = 9600;
        p->timeout = STD_TIMEOUT;
        p->in_use = FALSE;
        p->ibuf_len = p->obuf_len = 0;

        return p;
}

void gbp_delete(gbp_com *p)
{
        if (p != NULL) {
                if (p->in_use) TL_Close();
                if (p->tty != NULL) free(p->tty);
                free(p);
        }
}

static void gbp_free(void *p)
{
        gbp_delete(p);
}

static VALUE gbp_alloc(VALUE klass)
{
        gbp_com *gbp;
        VALUE obj;

        gbp = gbp_new();
        obj = Data_Wrap_Struct(klass, 0, gbp_free, gbp);

        return obj;
}

static void set_max_speed()
{
        unsigned char ibuf[] = { 0x0A, 0x04 }, obuf[GBP_BUFSIZE];
        unsigned char ibuf_len = 2, obuf_len;

        TL_SendReceive(ibuf_len, ibuf, &obuf_len, obuf);
}

static VALUE gbp_initialize(VALUE self, VALUE port)
{
        gbp_com *gbp;
        short int status;

        Data_Get_Struct(self, gbp_com, gbp);

        if (TYPE(port) == T_STRING) {
                if (gbp->tty != NULL)
                        free(gbp->tty);
                gbp->tty = StringValuePtr(port);
                status = TL_Open_TTY(gbp->tty, 9600);
                set_max_speed();
                /* TL_Close();
                status = TL_Open_TTY(gbp->tty, 38400); */
        } else {
                gbp->ncom = NUM2CHR(port);
                status = TL_Open(gbp->ncom, gbp->vcom);
        }
        if (status != NO_ERR) {
                gbp->in_use = FALSE;
        } else {
                gbp->in_use = TRUE;
        }
        TL_SetTimeOut(gbp->timeout);

        return self;
}

static VALUE gbp_init_copy(VALUE copy, VALUE orig)
{
        gbp_com *orig_gbp;
        gbp_com *copy_gbp;

        if (copy == orig)
                return copy;

        if (TYPE(orig) != T_DATA || RDATA(orig)->dfree != (RUBY_DATA_FUNC) gbp_free) {
                rb_raise(rb_eTypeError, "wrong argument type");
        }

        Data_Get_Struct(orig, gbp_com, orig_gbp);
        Data_Get_Struct(copy, gbp_com, copy_gbp);

        MEMCPY(copy_gbp, orig_gbp, gbp_com, 1);

        return copy;
}

static char *get_err_msg(int status)
{
        char *m = NULL;

        switch(status) {
        case ERR_COMNUM:
                m = "wrong tty";
                break;
        case ERR_INITPORT:
                m = "argument wrong";
                break;
        case ERR_LRC:
                m = "wrong LRC";
                break;
        case ERR_READER_MUTE:
                m = "reader is mute";
                break;
        case ERR_SEQUENCE_NUMBER:
                m = "incorrect sequence number";
                break;
        case ERR_RESYNCH:
                m = "resync command sent";
                break;
        case ERR_WRITEPORT:
                m = "error during send";
                break;
        case ERR_READPORT:
                m = "error during write";
                break;
        case ERR_SERIOUS_ERROR:
                m = "serious error, resync failed";
                break;
        case ERR_NACK:
                m = "NACK received from reader";
                break;
        case ERR_PARITY:
                m = "parity error";
                break;
        default:
                m = "unknown error code";
        }

        return m;
}

static VALUE gbp_send(VALUE self, VALUE bytes)
{
        gbp_com *gbp;
        int i;
        short int status;
        VALUE ary;

        Check_Type(bytes, T_ARRAY);
        Data_Get_Struct(self, gbp_com, gbp);

        for (i = 0; i < RARRAY_LEN(bytes); i++) {
                gbp->ibuf[i] = (unsigned char) FIX2INT(RARRAY_PTR(bytes)[i]);
        }
        gbp->ibuf_len = RARRAY_LEN(bytes);

        status = TL_SendReceive(gbp->ibuf_len, gbp->ibuf, &gbp->obuf_len, gbp->obuf);
        if (status != NO_ERR) {
                rb_raise(rb_eRuntimeError, get_err_msg(status));
        }

        ary = rb_ary_new2(gbp->obuf_len);

        for (i = 0; i < gbp->obuf_len; i++) {
                rb_ary_push(ary, CHR2FIX(gbp->obuf[i]));
        }

        return ary;
}

void Init_gbp()
{
        mChipcard = rb_define_module("Chipcard");
        mGBP = rb_define_module_under(mChipcard, "GBP");

        cReader = rb_define_class("Reader", rb_cObject);
        rb_define_alloc_func(cReader, gbp_alloc);
        rb_define_method(cReader, "initialize", gbp_initialize, 1);
        rb_define_method(cReader, "initialize_copy", gbp_init_copy, 1);
        rb_define_method(cReader, "send", gbp_send, 1);
}
