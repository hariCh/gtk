/* GTK - The GIMP Toolkit
 * Copyright (C) 2014, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */
#include "config.h"
#include <gtk/gtkgesturepan.h>
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkintl.h"

typedef struct _GtkGesturePanPrivate GtkGesturePanPrivate;

struct _GtkGesturePanPrivate
{
  guint orientation : 2;
  guint panning     : 1;
};

enum {
  PROP_ORIENTATION = 1
};

enum {
  PAN,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkGesturePan, gtk_gesture_pan, GTK_TYPE_GESTURE_DRAG)

static void
gtk_gesture_pan_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkGesturePanPrivate *priv;

  priv = gtk_gesture_pan_get_instance_private (GTK_GESTURE_PAN (object));

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_pan_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_gesture_pan_set_orientation (GTK_GESTURE_PAN (object),
                                       g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
direction_from_offset (gdouble            offset_x,
                       gdouble            offset_y,
                       GtkPanOrientation  orientation,
                       GtkPanDirection   *direction)
{
  if (orientation == GTK_PAN_ORIENTATION_HORIZONTAL)
    {
      if (offset_x > 0)
        *direction = GTK_PAN_DIRECTION_RIGHT;
      else
        *direction = GTK_PAN_DIRECTION_LEFT;
    }
  else if (orientation == GTK_PAN_ORIENTATION_VERTICAL)
    {
      if (offset_y > 0)
        *direction = GTK_PAN_DIRECTION_DOWN;
      else
        *direction = GTK_PAN_DIRECTION_UP;
    }
  else
    g_assert_not_reached ();
}

static gboolean
guess_direction (GtkGesturePan   *gesture,
                 gdouble          offset_x,
                 gdouble          offset_y,
                 GtkPanDirection *direction)
{
  gdouble abs_x, abs_y;

  abs_x = ABS (offset_x);
  abs_y = ABS (offset_y);

#define FACTOR 3
  if (abs_x > abs_y * FACTOR)
    direction_from_offset (offset_x, offset_y,
                           GTK_PAN_ORIENTATION_HORIZONTAL, direction);
  else if (abs_y > abs_x * FACTOR)
    direction_from_offset (offset_x, offset_y,
                           GTK_PAN_ORIENTATION_VERTICAL, direction);
  else
    return FALSE;

  return TRUE;
#undef FACTOR
}

static gboolean
check_orientation_matches (GtkGesturePan   *gesture,
                           GtkPanDirection  direction)
{
  GtkGesturePanPrivate *priv = gtk_gesture_pan_get_instance_private (gesture);

  if (priv->orientation == 0)
    return FALSE;

  return (((direction == GTK_PAN_DIRECTION_LEFT ||
            direction == GTK_PAN_DIRECTION_RIGHT) &&
           priv->orientation == GTK_PAN_ORIENTATION_HORIZONTAL) ||
          ((direction == GTK_PAN_DIRECTION_UP ||
            direction == GTK_PAN_DIRECTION_DOWN) &&
           priv->orientation == GTK_PAN_ORIENTATION_VERTICAL));
}

static void
gtk_gesture_pan_drag_update (GtkGestureDrag *gesture,
                             gdouble         offset_x,
                             gdouble         offset_y)
{
  GtkGesturePanPrivate *priv;
  GtkPanDirection direction;
  GtkGesturePan *pan;
  gdouble offset;

  pan = GTK_GESTURE_PAN (gesture);
  priv = gtk_gesture_pan_get_instance_private (pan);

  if (!priv->panning)
    {
      if (!guess_direction (pan, offset_x, offset_y, &direction))
        return;

      if (!check_orientation_matches (pan, direction))
        {
          gtk_gesture_set_state (GTK_GESTURE (gesture),
                                 GTK_EVENT_SEQUENCE_DENIED);
          return;
        }

      priv->panning = TRUE;
    }
  else
    direction_from_offset (offset_x, offset_y, priv->orientation, &direction);

  offset = (priv->orientation == GTK_PAN_ORIENTATION_VERTICAL) ?
    ABS (offset_y) : ABS (offset_x);
  g_signal_emit (gesture, signals[PAN], 0, direction, offset);
}

static void
gtk_gesture_pan_drag_end (GtkGestureDrag *gesture,
                          gdouble         offset_x,
                          gdouble         offset_y)
{
  GtkGesturePanPrivate *priv;

  priv = gtk_gesture_pan_get_instance_private (GTK_GESTURE_PAN (gesture));
  priv->panning = FALSE;
}

static void
gtk_gesture_pan_class_init (GtkGesturePanClass *klass)
{
  GtkGestureDragClass *drag_gesture_class = GTK_GESTURE_DRAG_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_gesture_pan_get_property;
  object_class->set_property = gtk_gesture_pan_set_property;

  drag_gesture_class->drag_update = gtk_gesture_pan_drag_update;
  drag_gesture_class->drag_end = gtk_gesture_pan_drag_end;

  g_object_class_install_property (object_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
                                                      P_("Orientation"),
                                                      P_("Allowed orientations"),
                                                      GTK_TYPE_PAN_ORIENTATION,
                                                      GTK_PAN_ORIENTATION_HORIZONTAL,
                                                      GTK_PARAM_READWRITE));
  signals[PAN] =
    g_signal_new ("pan",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGesturePanClass, pan),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_PAN_DIRECTION);
}

static void
gtk_gesture_pan_init (GtkGesturePan *gesture)
{
  GtkGesturePanPrivate *priv;

  priv = gtk_gesture_pan_get_instance_private (gesture);
  priv->orientation = GTK_PAN_ORIENTATION_HORIZONTAL;
}

GtkGesture *
gtk_gesture_pan_new (GtkWidget         *widget,
                     GtkPanOrientation  orientation)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_GESTURE_PAN,
                       "widget", widget,
                       "orientation", orientation,
                       NULL);
}

GtkPanOrientation
gtk_gesture_pan_get_orientation (GtkGesturePan *gesture)
{
  GtkGesturePanPrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_PAN (gesture), 0);

  priv = gtk_gesture_pan_get_instance_private (gesture);

  return priv->orientation;
}

void
gtk_gesture_pan_set_orientation (GtkGesturePan     *gesture,
                                 GtkPanOrientation  orientation)
{
  GtkGesturePanPrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_PAN (gesture));
  g_return_if_fail (orientation == GTK_PAN_ORIENTATION_HORIZONTAL ||
                    orientation == GTK_PAN_ORIENTATION_VERTICAL);

  priv = gtk_gesture_pan_get_instance_private (gesture);

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;
  g_object_notify (G_OBJECT (gesture), "orientation");
}