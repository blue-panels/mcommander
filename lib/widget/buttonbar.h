
/** \file buttonbar.h
 *  \brief Header: WButtonBar widget
 */

#ifndef MC__WIDGET_BUTTONBAR_H
#define MC__WIDGET_BUTTONBAR_H

/*** typedefs(not structures) and defined constants **********************************************/

#define BUTTONBAR(x) ((WButtonBar *) (x))

/* number of bttons in buttonbar */
#define BUTTONBAR_LABELS_NUM                 10

#define buttonbar_clear_label(bb, idx, recv) buttonbar_set_label (bb, idx, "", NULL, recv)

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* The label of a command on the key bar for a held modifier; text is a "ButtonBar|..." msgid */
typedef struct
{
    long command;
    const char *text;
} buttonbar_command_label_t;

typedef struct WButtonBar
{
    Widget widget;

    struct
    {
        char *text;
        long command;
        Widget *receiver;
        int end_coord;  // cumulative width of buttons so far
    } labels[BUTTONBAR_LABELS_NUM];

    /* With a modifier held: the keymaps its F keys are looked up in, the labels of the commands */
    const global_keymap_t *const *mod_keymaps[2];
    const buttonbar_command_label_t *mod_labels;
    Widget *mod_receiver;
} WButtonBar;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WButtonBar *buttonbar_new (void);
void buttonbar_set_label (WButtonBar *bb, int idx, const char *text, const global_keymap_t *keymap,
                          Widget *receiver);
WButtonBar *buttonbar_find (const WDialog *h);
void buttonbar_set_modifier_labels (WButtonBar *bb, const global_keymap_t *const *keymap,
                                    const global_keymap_t *const *keymap2,
                                    const buttonbar_command_label_t *labels, Widget *receiver);

/*** inline functions ****************************************************************************/

#endif
