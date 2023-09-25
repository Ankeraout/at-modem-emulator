#ifndef __INCLUDE_LIST_H__
#define __INCLUDE_LIST_H__

struct ts_listElement {
    void *data;
    struct ts_listElement *next;
};

#endif
