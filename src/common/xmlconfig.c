/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "xmlconfig.h"


/*
 * configuration_attrib_remove
 */
void
configuration_attrib_remove (ConfigurationAttrib *attrib)
{
  ConfigurationAttrib *link;
  GList *list = NULL;
  GList *iter;

  if (attrib == NULL)		/* do nothing .. */
    return;

  /* Construct list from attribute chain. */
  for (link = attrib; link != NULL; link = link->next)
    list = g_list_append (list, link);

  /* Deallocate each attribute link in the list. */
  for (iter = list; iter != NULL; iter = iter->next) {
    link = (ConfigurationAttrib *)iter->data;
    g_free (link->name);
    g_free (link->value);
    g_free (link);
  }

  /* Deallocate attribute chain list. */
  g_list_free (list);
} /* </configuration_attrib_remove> */

/*
 * configuration_attrib
 * configuration_attrib_string
 */
gchar *
configuration_attrib (ConfigurationNode *node, gchar *name)
{
  ConfigurationAttrib *attrib = node->attrib;
  gchar *value = NULL;

  if (attrib && attrib->name)
    while (attrib != NULL) {
      if (strcmp(attrib->name, name) == 0) {
        value = attrib->value;
        break;
      }
      attrib = attrib->next;
    }

  return value;
} /* </configuration_attrib> */

gchar *
configuration_attrib_string (ConfigurationNode *node)
{
  ConfigurationAttrib *attrib = node->attrib;
  static gchar line[MAX_PATHNAME];
  gchar *string = NULL;

  if (attrib != NULL) {
    strcpy(line, attrib->name);
    strcat(line, "=\"");
    strcat(line, attrib->value);
    strcat(line, "\"");

    for (attrib = attrib->next; attrib != NULL; attrib = attrib->next) {
      strcat(line, " ");
      strcat(line, attrib->name);
      strcat(line, "=\"");
      strcat(line, attrib->value);
      strcat(line, "\"");
    }
    string = line;
  }
  return string;
} /* </configuration_attrib_string> */

/*
 * configuration_find
 * configuration_find_end
 * configuration_find_next
 */
ConfigurationNode *
configuration_find (ConfigurationNode *node, const gchar *name)
{
  ConfigurationNode *item;

  for (item = node; item != NULL; item = item->next) {
    if (item->depth < node->depth || item->element == NULL) {
      item = NULL;
      break;
    }
    if (strcmp(item->element, name) == 0) {
      if (item->next && item->next->type == XML_READER_TYPE_ATTRIBUTE)
        item = item->next;
      break;
    }
  }
  return item;
} /* </configuration_find> */

ConfigurationNode *
configuration_find_end (ConfigurationNode *node)
{
  ConfigurationNode *chain = node->next;
  ConfigurationNode *item  = NULL;

  if (chain != NULL) {
    if (strcmp(chain->element, "/") == 0)	/* handle <#text /> */
      item = chain;
    else {
      gchar *match = g_strdup_printf ("/%s", node->element);

      for ( ; chain != NULL; chain = chain->next)
        if (strcmp(chain->element, match) == 0) {
          item = chain;
          break;
        }

      g_free (match);
    }
  }
  return item;
} /* </configuration_find_end> */

ConfigurationNode *
configuration_find_next (ConfigurationNode *node, const gchar *name)
{
  ConfigurationNode *chain = configuration_find (node, name);
  ConfigurationNode *item  = NULL;

  if (chain != NULL) {
    guint depth = chain->depth;

    for ( ; chain != NULL; chain = chain->next)
      if (chain->type == XML_READER_TYPE_END_ELEMENT && chain->depth == depth) {
        item = chain->next;
        break;
      }
  }
  return item;
} /* </configuration_find_next> */

/*
 * configuration_insert
 * configuration_remove
 * configuration_move
 */
ConfigurationNode *
configuration_insert (ConfigurationNode *node,
                      ConfigurationNode *site,
                      gint nesting)
{
  const char *ident = "[configuration_insert]";

  ConfigurationNode *mark = site->next;	 /* save node insertion forward link */
  ConfigurationNode *tail = configuration_find_end (node);

  /* Worst case point tail to node itself.. should never be. */
  if (tail == NULL) {
    fprintf(stderr, "%s%s.\n", ident, _("cannot see end tag of node inserted"));
    tail = node;
  }

  site->next = node;
  tail->next = mark;
  mark->back = tail;
  node->back = site;

  if (nesting != 0) {		/* correct nesting depth */
    ConfigurationNode *iter = node;

    for (mark = tail->next; iter != mark; iter = iter->next)
      iter->depth += nesting; 
  }

  return mark;
} /* </configuration_insert> */

ConfigurationNode *
configuration_remove (ConfigurationNode *node)
{
  ConfigurationNode *chain;
  ConfigurationNode *trail;
  ConfigurationNode *item;

  ConfigurationNode *mark = configuration_find_end (node);
  guint depth = node->depth;

  GList *list = NULL;
  GList *iter;

  if (mark == NULL)	/* account for <... /> element */
    mark = node;

  /* Prune and graft to previous node and after the end element. */
  chain = (node->back) ? node->back : node;
  trail = (mark->next) ? mark->next : mark;

  chain->next = trail;
  trail->back = chain;

  /* Create forward list of the chain of nodes. */
  for (item = node; item != mark->next; item = item->next)
    list = g_list_append (list, item);

  /* Iterate list of the nodes chain and free memory. */
  for (iter = list; iter != NULL; iter = iter->next) {
    item = (ConfigurationNode *)iter->data;
    configuration_attrib_remove (item->attrib);
    g_free (item->element);
    g_free (item);
  }

  g_list_free (list);	/* done.. free forward list */

  /* Find the previous element to return. */
  if (chain->depth < depth)
    mark = chain;
  else
    for (mark = chain->back; mark && mark->depth > depth; )
      mark = mark->back;

  return mark;
} /* </configuration_remove> */

ConfigurationNode *
configuration_move (ConfigurationNode *node, ConfigurationNode *site)
{
  ConfigurationNode *mark = node->back;
  ConfigurationNode *tail = configuration_find_end (node);

  if (node == site)		/* avoid unnecessary work... */
    return site;

  /* Compensate for <... /> cases. */
  if (tail == NULL)
    tail = node;

  /* Adjust node links for the move. */
  mark->next = tail->next;
  tail->next->back = mark;

  /* Place node at site position. */
  mark = site->next;		/* save node insertion forward link */

  site->next = node;
  tail->next = mark;
  mark->back = tail;
  node->back = site;

  if (node->depth != site->depth) {	/* correct nesting depth */
    ConfigurationNode *iter = node;
    gint adjust = site->depth - node->depth;

    for (mark = tail->next; iter != mark; iter = iter->next)
      iter->depth += adjust; 
  }

  return site;
} /* </configuration_move> */

/*
 * configuration_clone duplicates a ConfigurationNode tree
 */
ConfigurationNode *
configuration_clone (ConfigurationNode *node)
{
  ConfigurationNode *clone = NULL;
  ConfigurationNode *marker;	/* end of chain marker */
  gchar tail[MAX_PATHNAME];

  sprintf(tail, "/%s", node->element);
  marker = configuration_find (node, tail);  /* corresponding end element */

  if (marker != NULL) {
    ConfigurationNode *chain;
    ConfigurationNode *back;
    ConfigurationNode *item;

    /* Start and end of the node chain. */
    marker = marker->next;
    clone = g_new0(ConfigurationNode, 1);
    back = item = clone;

    for (chain = node; chain != marker; chain = chain->next) {
      item->data    = chain->data;
      item->depth   = chain->depth;
      item->element = g_strdup (chain->element);
      item->type    = chain->type;

      if (chain->attrib) {
        ConfigurationAttrib *attrib, *iter;
        attrib = item->attrib = g_new0(ConfigurationAttrib, 1);

        for (iter = chain->attrib; iter != NULL; iter = iter->next) {
          attrib->name = g_strdup (iter->name);
          attrib->value = g_strdup (iter->value);

          if (iter->next != NULL) {
            attrib = attrib->next = g_new0(ConfigurationAttrib, 1);
          }
        }
      }

      if (item != clone)	/* set back and next links */
        item->back = back;

      if (chain->next != marker) {
        back = item;
        item = item->next = g_new0(ConfigurationNode, 1);
      }
    }
  }

  return clone;
} /* </configuration_clone> */

/*
 * configuration_path
 */
gchar *
configuration_path (ConfigurationNode *config)
{
  ConfigurationNode *item = configuration_find (config, "path");
  gchar *path = NULL;

  if (item != NULL) {
    if ((item->element)[0] == '$') {	/* getenv (&(item->element)[1]) */
      char dirname[MAX_PATHNAME];
      char *scan;

      strcpy(dirname, item->element);
      scan = strchr(dirname, '/');

      if (scan) {
        *scan = (char)0;
        path = g_strdup_printf("%s/%s", getenv(&dirname[1]), scan+1);
      }
      else {
        path = getenv(&dirname[1]);
      }
    }
    else {
      path = item->element;
    }
  }
  return path;
} /* </configuration_path> */

/*
 * (private) configuration_read_attributes
 */
static ConfigurationAttrib *
configuration_read_attributes (xmlTextReaderPtr reader)
{
  int count = xmlTextReaderAttributeCount(reader);
  ConfigurationAttrib *attrib = g_new0(ConfigurationAttrib, 1);
  ConfigurationAttrib *chain  = attrib;
  int idx;

  for (idx = 0; idx < count; idx++) {
    xmlTextReaderMoveToAttributeNo(reader, idx);

    chain->name  = g_strdup ((gchar *)xmlTextReaderConstName(reader));
    chain->value = g_strdup ((gchar *)xmlTextReaderConstValue(reader));

    if (idx < count - 1)
      chain = chain->next = g_new0(ConfigurationAttrib, 1);
  }
  xmlTextReaderMoveToElement(reader);	/* need to move back to the element */

  return attrib;
} /* </configuration_read_attributes> */

/*
 * configuration_read
 */
static gboolean xmlerror_ = FALSE;

static void
configuration_read_error (void *context, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  vfprintf (stderr, format, args);
  xmlerror_ = TRUE;
  va_end (args);
} /* </configuration_read_error> */

ConfigurationNode *
configuration_read (const gchar *data, const gchar *schema, gboolean memory)
{
  ConfigurationNode *config = NULL;
  ConfigurationNode *item, *node = NULL;
  xmlTextReaderPtr reader;

  if (memory)
    reader = xmlReaderForMemory (data, strlen(data), NULL, schema, 0);
  else
    reader = xmlReaderForFile(data, NULL, 0);

  if (reader) {		/* Pass 1: Read the XML specification */
    const xmlChar *name, *value;
    int status = xmlTextReaderRead(reader);

    if (status < 0) {
      fprintf(stderr, "[configuration_read]%s\n", _("invalid xmlReaderTypes"));
      return NULL;
    }

    /* Set the xmlGenericErrorFunc handler. */
    xmlSetGenericErrorFunc (0, (xmlGenericErrorFunc)configuration_read_error);

    if (memory == FALSE)	/* we are reading from a file */
      if (strcmp((char *)xmlTextReaderConstName(reader), schema)) {
        fprintf(stderr, "[configuration_read]%s: %s\n",
                          _("invalid or corrupted XML schema"), data);
        return NULL;
      }

    /* Start a new ConfigurationNode data structure. */
    config = item = g_new0(ConfigurationNode, 1);

    if (xmlTextReaderHasAttributes(reader))
      config->attrib = configuration_read_attributes (reader);

    while (status == XML_TEXTREADER_MODE_INTERACTIVE) {
      name  = xmlTextReaderConstName(reader);
      value = xmlTextReaderConstValue(reader);

      item->depth = xmlTextReaderDepth(reader);
      item->type = xmlTextReaderNodeType(reader);

      if (item->type != XML_READER_TYPE_COMMENT) {
        if (xmlTextReaderHasValue(reader)) {  /* separate name and value text */
          if (value[0] != '\n') {
            item->type = XML_READER_TYPE_ATTRIBUTE;
            item->element = g_strdup((gchar *)value);
          }
        }
        else {
          if (item->type == XML_READER_TYPE_END_ELEMENT)
            item->element = g_strdup_printf("/%s", (gchar *)name);
          else
            item->element = g_strdup((gchar *)name);
        }

        if (xmlTextReaderHasAttributes(reader)) {
          if (item->type != XML_READER_TYPE_END_ELEMENT)
            item->attrib = configuration_read_attributes (reader);
        }

        /* allocate a ConfigurationNode for the next record */
        if (item->element) {
          node = item;
          item->next = g_new0(ConfigurationNode, 1);
          item = item->next;
          item->back = node;
        }
      }
      status = xmlTextReaderRead(reader);

      if (xmlerror_) {			/* configuration_read_error called */
        xmlFreeTextReader(reader);	/* free the XML text reader */
        xmlerror_ = FALSE;
        return NULL;			/* FIXME: free up memory */
      }
    }

    /* Last node in the chain is redundant, and could cause errors. */
    if (node->element) {
      node->next = NULL;
      g_free (item);
    }

    xmlFreeTextReader(reader);	/* free the XML text reader */
  }

  if (config != NULL) {	/* Pass 2: Add XML_READER_TYPE_END_ELEMENT as needed */
    gboolean missing;

    for (item = config; item != NULL; item = item->next) {
      node = item->next;
      missing = FALSE;	/* start assuming no addition needed */

      if (node && item->type == XML_READER_TYPE_ELEMENT) {
        if (node->type != XML_READER_TYPE_END_ELEMENT)
          missing = (node->depth == item->depth);
        else
          missing = (node->depth == (item->depth - 1));
      }

      if (item->back == NULL && item->next == NULL)
        missing = TRUE;

      if (missing) {
        node = g_new0(ConfigurationNode, 1);

        node->type = XML_READER_TYPE_END_ELEMENT;
        node->element = g_strdup("/");
        node->depth = item->depth;

        node->next = item->next;
        node->back = item;
        item->next = node;

        item = item->next;
      }
    }
  }

  return config;
} /* </configuration_read> */

/*
 * configuration_replace
 */
void
configuration_replace (ConfigurationNode *config, gchar *data,
                       const char *header, const char *section,
                       const char *ident)
{
  ConfigurationNode *site  = NULL;
  ConfigurationNode *chain = configuration_find (config, header);
  ConfigurationNode *item;

  gchar *trailer = g_strdup_printf ("/%s", header);
  const gchar *attrib;


  /* See if the configuration item already exists within the section. */
  while ((item = configuration_find (chain, section)) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, ident) == 0) {
      site = item->back;
      configuration_remove (item);  /* remove original configuration item */
      break;
    }

    chain = configuration_find_next (chain, section);
  }

  if (site == NULL) {
    chain = configuration_find (config, trailer);
    site = chain->back;
  }

   /* Create new item and insert into configuration cache. */
  item = configuration_read (data, NULL, TRUE);
  configuration_insert (item, site, site->depth);

  g_free (trailer);
} /* </configuration_replace> */

/*
 * configuration_write
 */
int
configuration_write (ConfigurationNode *config,
                     const char *header,
                     FILE *stream)
{
  const char *Header =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<%s>\n"
"\n";
  const char *padding = "                  ";

  ConfigurationNode *item = config;
  ConfigurationNode *marker = configuration_find_end (item);
  ConfigurationNode *node;

  gchar *attrib;
  char   indent[strlen(padding)];
  char   schema[MAX_PATHNAME];
  int    bytes = 0;

  if (config->attrib != NULL)
    sprintf(schema, "%s %s", config->element,
                             configuration_attrib_string (config));
  else
    strcpy(schema, config->element);

  bytes += fprintf(stream, ((header != NULL) ? header : Header), schema);
  item = item->next;

  /* Walk through all list elements. */
  for (; item != NULL; item = item->next) {
    attrib = configuration_attrib_string (item);
    strncpy(indent, padding, item->depth);
    indent[item->depth] = (char)0;
    node = item->next;		/* next configuration item */

    if (node && node->type == XML_READER_TYPE_ATTRIBUTE) {
      bytes += fprintf(stream, "%s<%s", indent, item->element);

      if (attrib != NULL)
        bytes += fprintf(stream, " %s>%s", attrib, node->element);
      else
        bytes += fprintf(stream, ">%s", node->element);

      bytes += fprintf(stream, "</%s>\n", item->element);
      item = node->next;
    }
    else if (item->element != NULL) {
      /* Add additional line at start of a section. */
      if (item->depth == 1 && item->type == XML_READER_TYPE_ELEMENT)
        fprintf(stream, "\n");

      if (attrib != NULL)
        bytes += fprintf(stream, "%s<%s %s", indent, item->element, attrib);
      else
        bytes += fprintf(stream, "%s<%s", indent, item->element);

      if (node && node->type == XML_READER_TYPE_END_ELEMENT &&
                                    node->depth == item->depth)
      {
        bytes += fprintf(stream, " />\n");
        item = item->next;
      }
      else
        bytes += fprintf(stream, ">\n");
    }

    /* Check ahead for the end of configuration. */
    if ((header && node == NULL) || item == marker)
      break;
  }
  return bytes;
} /* </configuration_write */

/*
 * configuration_schema_version
 */
SchemaVersion *
configuration_schema_version (ConfigurationNode *config)
{
  static SchemaVersion version;
  const char *value = configuration_attrib (config, "version");

  version.major = 0;
  version.minor = 0;
  version.patch = 0;

  if (value != NULL) {		/* release version specified */
    char *scan, *word;
    char string[MAX_BUFFER_SIZE];

    strcpy(string, value);
    scan = strchr(string, '.');

    if (scan != NULL) {		/* minor release specified */
      *scan++ = (char)0;
      word = strchr(scan, '.');

      if (word != NULL) {	/* patch level specified */
        *word++ = (char)0;
        version.patch = atoi(word);
      }
      version.minor = atoi(scan);
    }
    version.major = atoi(string);
  }

  return &version;
} /* </configuration_schema_version> */
