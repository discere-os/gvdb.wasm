/*
 * Copyright 2025 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include <glib.h>
#include "gvdb/gvdb-reader.h"

#define DEPTH 4

static void
gvdb_table_dump (GvdbTable *table,
                 int        depth)
{
  gchar **names;
  gsize length;

  names = gvdb_table_get_names (table, &length);
  if (!names)
    return;

  for (gsize i = 0; i < length; i++)
    {
      g_print ("%*c%s", depth, ' ', names[i]);

      if (gvdb_table_has_value (table, names[i]))
        {
          GVariant *var;
          GString *val;

          var = gvdb_table_get_value (table, names[i]);
          val = g_variant_print_string (var, NULL, FALSE);
          g_print (": %s\n", val->str);
          g_string_free (val, TRUE);
          g_variant_unref (var);
        }
      else
        {
          GvdbTable *table2;

          g_print ("\n");

          table2 = gvdb_table_get_table (table, names[i]);
          if (table2)
            {
              gvdb_table_dump (table2, depth + DEPTH);
              gvdb_table_free (table2);
            }
        }
    }

  g_strfreev (names);
}

int
main (int argc, char *argv[])
{
  GError *error = NULL;
  GvdbTable *table;
  const char *filename;

  if (argc != 2)
    {
      g_printerr ("Usage: gvdb-dump FILE\n");
      exit (1);
    }

  filename = argv[1];

  table = gvdb_table_new (filename, TRUE, &error);

  if (!table)
    g_error ("Can't read database from %s: %s", filename, error->message);

  gvdb_table_dump (table, 0);

  gvdb_table_free (table);

  return 0;
}
