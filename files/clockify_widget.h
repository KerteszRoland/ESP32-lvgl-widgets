/**
 * @file clockify_widget.h
 *
 */

#ifndef CLOCKIFY_WIDGET_H
#define CLOCKIFY_WIDGET_H

#ifdef __cplusplus
extern "C"
{
#endif

    /*********************
     *      INCLUDES
     *********************/

    /*********************
     *      DEFINES
     *********************/

    /**********************
     *      TYPEDEFS
     **********************/

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void render_clockify_widget(lv_obj_t *parent=nullptr);
    void start_clockify_widget_tasks(void);
    void stop_clockify_widget_tasks(void);
    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*CLOCKIFY_WIDGET_H*/
