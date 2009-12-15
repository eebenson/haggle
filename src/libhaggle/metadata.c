/* Copyright 2008 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *     
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */ 
#include "metadata.h"
#include <stdio.h>
#include <string.h>
#include "debug.h"

static inline void metadata_iterator_init(struct metadata_iterator *it, list_t *head, const char *name)
{
        if (!it)
                return;

        it->head = head;
        it->pos = it->head->next;
        it->tmp = it->pos->next;
	
        if (it->name)
                free(it->name);
        
        if (name && strlen(name) > 0) {
		
                it->name = (char *)malloc(strlen(name) + 1);
                
                if (!it->name)
                        return;
                
                strcpy(it->name, name);
		
		/* Iterate to first occurance of 'name' */
		for (; it->pos != it->head; it->pos = it->tmp, it->tmp = it->pos->next) {
			metadata_t *mc =  (metadata_t *)it->pos;
			
			if (strcmp(metadata_get_name(mc), name) == 0) {
				return;
			}
		}
        }
}

static inline int metadata_iterator_is_at_end(struct metadata_iterator *it)
{
        if (!it)
                return -1;

        if (it->pos == it->head)
                return 1;
        
        return 0;        
}

static inline int metadata_iterator_next(struct metadata_iterator *it)
{
        if (!it)
                return -1;

        if (it->pos == it->head)
                return 0;
        
        it->pos = it->tmp;
        it->tmp = it->pos->next;

        if (it->pos == it->head) {
                return 0;
        }

        if (!it->name)
                return 1;

        for (; it->pos != it->head; it->pos = it->tmp, it->tmp = it->pos->next) {
                metadata_t *mc =  (metadata_t *)it->pos;
                if (strcmp(metadata_get_name(mc), it->name) == 0)
                        return 1;
        }
        
        return 0;
}

static inline metadata_t *metadata_iterator_deref(struct metadata_iterator *it)
{
        if (!it || (it->pos == it->head))
                return NULL;

        return (metadata_t *)it->pos;
}

metadata_t *metadata_new(const char *name, const char *content, metadata_t *parent)
{
        metadata_t *m;

        m = (metadata_t*)malloc(sizeof(struct metadata));

        if (!m)
                return NULL;
        
	memset(m, 0, sizeof(struct metadata));
        
	INIT_LIST(&m->l);
	INIT_LIST(&m->children);

        if (parent)
                m->parent = parent;
        else
                m->parent = m;

        m->parameters = haggle_attributelist_new();

        if (!m->parameters) {
                free(m);
                return NULL;
        }

        metadata_set_name(m, name);
        metadata_set_content(m, content);

        return m;
}

void metadata_free(metadata_t *m)
{
        if (!m)
                return;

        list_detach(&m->l);

        /* Free all children recursively */
        while (!list_empty(&m->children)) {
                metadata_free((metadata_t *)list_first(&m->children));
        }

        if (m->name)
                free(m->name);

        if (m->content)
                free(m->content);

        if (m->it.name)
                free(m->it.name);

        haggle_attributelist_free(m->parameters);
        
        free(m);
}

int metadata_name_is(const metadata_t *m, const char *name)
{
        if (!m)
                return -1;

        if (m->name && strcmp(m->name, name) == 0)
                return 1;

        return 0;
}

const char *metadata_get_name(const metadata_t *m)
{
        return (m ? m->name : NULL);
}

const char *metadata_get_content(const metadata_t *m)
{
        return (m ? m->content : NULL);
}

const char *metadata_set_name(metadata_t *m, const char *name)
{
        char *tmpname;

        if (!m || !name)
                return NULL;

        tmpname = (char *)malloc(strlen(name) + 1);

        if (!tmpname)
                return NULL;

        if (m->name)
                free(m->name);

        m->name = tmpname;

        strcpy(m->name, name);

        return m->name;
}

const char *metadata_set_content(metadata_t *m, const char *content)
{
        char *tmpc;

        if (!m || !content)
                return NULL;

        tmpc = (char *)malloc(strlen(content) + 1);

        if (!tmpc)
                return NULL;

        if (m->content)
                free(m->content);

        m->content = tmpc;

        strcpy(m->content, content);

        return m->content;
}

#ifdef DEBUG
void metadata_print(metadata_t *m)
{
        list_t *pos, *tmp;

        LIBHAGGLE_DBG("parent \'%s:%s\'\n", 
               metadata_get_name(m), 
               metadata_get_content(m) ? metadata_get_content(m) : "no content");

        list_for_each_safe(pos, tmp, &m->parameters->attributes) {
                haggle_attr_t *a = (haggle_attr_t *)pos;
                LIBHAGGLE_DBG("param %s=%s ", haggle_attribute_get_name(a), haggle_attribute_get_value(a));
        }
        list_for_each_safe(pos, tmp, &m->children) {
                metadata_t *mc = (metadata_t *)pos;
                metadata_print(mc);
        }
}
#endif

metadata_t *metadata_get_next(metadata_t *m)
{
        if (!m || !m->it.head)
                return NULL;

        if (metadata_iterator_is_at_end(&m->it))
                return NULL;

        metadata_iterator_next(&m->it);
        
        return metadata_iterator_deref(&m->it);
}

metadata_t *metadata_get(metadata_t *m, const char *name)
{
        if (!m)
                return NULL;
        
        /* metadata_print(m); */
	
        metadata_iterator_init(&m->it, &m->children, name);

        return metadata_iterator_deref(&m->it);
}

int metadata_add(metadata_t *parent, metadata_t *child)
{
       if (!parent || !child)
                return -1;
                
        child->parent = parent;

        list_add(&child->l, &parent->children);
	
	return ++parent->num_children;
}

int metadata_detach(metadata_t *parent, metadata_t *child)
{
        if (!parent || !child)
                return -1;

        list_detach(&child->l);
        
        return --parent->num_children;
}

int metadata_set_parameter(metadata_t *m, const char *name, const char *value)
{
        struct attribute *a;

        if (!m || !name || !value)
                return -1;
        
        
        a = haggle_attributelist_get_attribute_by_name(m->parameters, name);

        if (a) {
                if (haggle_attribute_set_value(a, value))
                        return 0;
                else 
                        return -1;
        } 
        
        a = haggle_attribute_new(name, value);
        
        if (!a)
                return -1;

        haggle_attributelist_add_attribute(m->parameters, a);

        return 1;
}

const char *metadata_get_parameter(const metadata_t *m, const char *name)
{
        if (!m || !name)
                return NULL;
        
        return haggle_attribute_get_value(haggle_attributelist_get_attribute_by_name(m->parameters, name));
}
