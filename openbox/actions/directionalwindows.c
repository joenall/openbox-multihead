#include "openbox/actions.h"
#include "openbox/event.h"
#include "openbox/focus_cycle.h"
#include "openbox/openbox.h"
#include "openbox/misc.h"
#include "gettext.h"

typedef struct {
    gboolean interactive;
    gboolean dialog;
    gboolean dock_windows;
    gboolean desktop_windows;
    ObDirection direction;
    GSList *actions;
} Options;

static gboolean cycling = FALSE;

static gpointer setup_func(ObParseInst *i, xmlDocPtr doc, xmlNodePtr node);
static gpointer setup_cycle_func(ObParseInst *i, xmlDocPtr doc,
                                 xmlNodePtr node);
static gpointer setup_target_func(ObParseInst *i, xmlDocPtr doc,
                                  xmlNodePtr node);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);
static gboolean i_input_func(guint initial_state,
                             XEvent *e,
                             gpointer options,
                             gboolean *used);
static void     i_cancel_func(gpointer options);

static void     end_cycle(gboolean cancel, guint state, Options *o);

void action_directionalwindows_startup()
{
    actions_register("DirectionalCycleWindows", setup_cycle_func, free_func,
                     run_func, i_input_func, i_cancel_func);
    actions_register("DirectionalTargetWindow", setup_target_func, free_func,
                     run_func, NULL, NULL);
}

static gpointer setup_func(ObParseInst *i, xmlDocPtr doc, xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_new0(Options, 1);
    o->dialog = TRUE;

    if ((n = parse_find_node("dialog", node)))
        o->dialog = parse_bool(doc, n);
    if ((n = parse_find_node("panels", node)))
        o->dock_windows = parse_bool(doc, n);
    if ((n = parse_find_node("desktop", node)))
        o->desktop_windows = parse_bool(doc, n);
    if ((n = parse_find_node("direction", node))) {
        gchar *s = parse_string(doc, n);
        if (!g_ascii_strcasecmp(s, "north") ||
            !g_ascii_strcasecmp(s, "up"))
            o->direction = OB_DIRECTION_NORTH;
        else if (!g_ascii_strcasecmp(s, "northwest"))
            o->direction = OB_DIRECTION_NORTHWEST;
        else if (!g_ascii_strcasecmp(s, "northeast"))
            o->direction = OB_DIRECTION_NORTHEAST;
        else if (!g_ascii_strcasecmp(s, "west") ||
                 !g_ascii_strcasecmp(s, "left"))
            o->direction = OB_DIRECTION_WEST;
        else if (!g_ascii_strcasecmp(s, "east") ||
                 !g_ascii_strcasecmp(s, "right"))
            o->direction = OB_DIRECTION_EAST;
        else if (!g_ascii_strcasecmp(s, "south") ||
                 !g_ascii_strcasecmp(s, "down"))
            o->direction = OB_DIRECTION_SOUTH;
        else if (!g_ascii_strcasecmp(s, "southwest"))
            o->direction = OB_DIRECTION_SOUTHWEST;
        else if (!g_ascii_strcasecmp(s, "southeast"))
            o->direction = OB_DIRECTION_SOUTHEAST;
        g_free(s);
    }

    if ((n = parse_find_node("finalactions", node))) {
        xmlNodePtr m;

        m = parse_find_node("action", n->xmlChildrenNode);
        while (m) {
            ObActionsAct *action = actions_parse(i, doc, m);
            if (action) o->actions = g_slist_prepend(o->actions, action);
            m = parse_find_node("action", m->next);
        }
    }
    else {
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Focus"));
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Raise"));
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Unshade"));
    }

    return o;
}

static gpointer setup_cycle_func(ObParseInst *i, xmlDocPtr doc,
                                 xmlNodePtr node)
{
    Options *o = setup_func(i, doc, node);
    o->interactive = TRUE;
    return o;
}

static gpointer setup_target_func(ObParseInst *i, xmlDocPtr doc,
                                  xmlNodePtr node)
{
    Options *o = setup_func(i, doc, node);
    o->interactive = FALSE;
    return o;
}

static void free_func(gpointer options)
{
    Options *o = options;

    while (o->actions) {
        actions_act_unref(o->actions->data);
        o->actions = g_slist_delete_link(o->actions, o->actions);
    }

    g_free(o);
}

static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;

    if (!o->interactive)
        end_cycle(FALSE, data->state, o);
    else {
        focus_directional_cycle(o->direction,
                                o->dock_windows,
                                o->desktop_windows,
                                TRUE,
                                o->dialog,
                                FALSE, FALSE);
        cycling = TRUE;
    }

    return o->interactive;
}

static gboolean i_input_func(guint initial_state,
                             XEvent *e,
                             gpointer options,
                             gboolean *used)
{
    if (e->type == KeyPress) {
        /* Escape cancels no matter what */
        if (e->xkey.keycode == ob_keycode(OB_KEY_ESCAPE)) {
            end_cycle(TRUE, e->xkey.state, options);
            return FALSE;
        }

        /* There were no modifiers and they pressed enter */
        else if (e->xkey.keycode == ob_keycode(OB_KEY_RETURN) &&
                 !initial_state)
        {
            end_cycle(FALSE, e->xkey.state, options);
            return FALSE;
        }
    }
    /* They released the modifiers */
    else if (e->type == KeyRelease && initial_state &&
             (e->xkey.state & initial_state) == 0)
    {
        end_cycle(FALSE, e->xkey.state, options);
        return FALSE;
    }

    return TRUE;
}

static void i_cancel_func(gpointer options)
{
    /* we get cancelled when we move focus, but we're not cycling anymore, so
       just ignore that */
    if (cycling)
        end_cycle(TRUE, 0, options);
}

static void end_cycle(gboolean cancel, guint state, Options *o)
{
    struct _ObClient *ft;

    ft = focus_directional_cycle(o->direction,
                                 o->dock_windows,
                                 o->desktop_windows,
                                 o->interactive,
                                 o->dialog,
                                 TRUE, cancel);
    cycling = FALSE;

    if (ft) {
        actions_run_acts(o->actions, OB_USER_ACTION_KEYBOARD_KEY,
                         state, -1, -1, 0, OB_FRAME_CONTEXT_NONE, ft);
    }
}