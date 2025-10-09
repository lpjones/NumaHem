#include <pthread.h>
#include <stdlib.h>

#include "tmem.h"
#include "fifo.h"

void enqueue_fifo(struct fifo_list *queue, struct tmem_page *entry)
{
  if (queue == &hot_list) {
    LOG_DEBUG("enqueue_fifo(hot, %p) %lu\n", entry, queue->numentries);
  }
  else if (queue == &cold_list) {
    LOG_DEBUG("enqueue_fifo(cold, %p) %lu\n", entry, queue->numentries);
  }
  else if (queue == &free_list) {
    LOG_DEBUG("enqueue_fifo(free, %p) %lu\n", entry, queue->numentries);
  }
  else 
    LOG_DEBUG("enqueue_fifo(%p, %p) %lu\n", queue, entry, queue->numentries);

  pthread_mutex_lock(&(queue->list_lock));
  assert(entry->list == NULL);
  assert(entry->prev == NULL);
  entry->next = queue->first;
  if(queue->first != NULL) {
    assert(queue->first->prev == NULL);
    queue->first->prev = entry;
  } else {
    assert(queue->last == NULL);
    assert(queue->numentries == 0);
    queue->last = entry;
  }

  queue->first = entry;
  entry->list = queue;
  // queue->numentries++;
  __atomic_fetch_add(&queue->numentries, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&(queue->list_lock));
}

struct tmem_page *dequeue_fifo(struct fifo_list *queue)
{
  // Check atomic numentries first to not lock every time for empty queue
  if (__atomic_load_n(&queue->numentries, __ATOMIC_ACQUIRE) == 0) {
    return NULL;
  }
  if (queue == &hot_list) {
    LOG_DEBUG(" dequeue_fifo(hot, ");
  }
  else if (queue == &cold_list) {
    LOG_DEBUG(" dequeue_fifo(cold, ");
  }
  else if (queue == &free_list) {
    LOG_DEBUG(" dequeue_fifo(free, ");
  }
  else 
    LOG_DEBUG(" dequeue_fifo(%p, ", queue);
  
  pthread_mutex_lock(&(queue->list_lock));
  struct tmem_page *ret = queue->last;

  // pthread_mutex_lock(&ret->page_lock);
  if (ret == NULL || ret->list != queue) {
    // pthread_mutex_unlock(&ret->page_lock);
    pthread_mutex_unlock(&queue->list_lock);
    return NULL;
  }

  queue->last = ret->prev;
  if(queue->last != NULL) {
    queue->last->next = NULL;
  } else {
    queue->first = NULL;
  }

  ret->prev = ret->next = NULL;
  ret->list = NULL;
  assert(queue->numentries > 0);
  // queue->numentries--;
  __atomic_fetch_sub(&queue->numentries, 1, __ATOMIC_RELEASE);

  // pthread_mutex_unlock(&ret->page_lock); // caller must unlock page
  pthread_mutex_unlock(&(queue->list_lock));
  LOG_DEBUG("%p) %lu\n", ret, queue->numentries);

  return ret;
}

void page_list_remove_page(struct fifo_list *list, struct tmem_page *page)
{
  if (list == &hot_list) {
    LOG_DEBUG("  page_list_remove_page(hot, %p) %lu\n", page, list->numentries);
  }
  else if (list == &cold_list) {
    LOG_DEBUG("  page_list_remove_page(cold, %p) %lu\n", page, list->numentries);
  }
  else if (list == &free_list) {
    LOG_DEBUG("  page_list_remove_page(free, %p) %lu\n", page, list->numentries);
  }
  else 
    LOG_DEBUG("  page_list_remove_page(%p, %p) %lu\n", list, page, list->numentries);

  pthread_mutex_lock(&(list->list_lock));
  if (page->list != list) {
    pthread_mutex_unlock(&list->list_lock);
    return;
  }
  if (list->first == NULL) {
    assert(list->last == NULL);
    assert(list->numentries == 0);
    pthread_mutex_unlock(&(list->list_lock));
    LOG_DEBUG("page_list_remove_page: list was empty!\n");
    return;
  }

  if (list->first == page) {
    list->first = page->next;
  }

  if (list->last == page) {
    list->last = page->prev;
  }

  if (page->next != NULL) {
    page->next->prev = page->prev;
  }

  if (page->prev != NULL) {
    page->prev->next = page->next;
  }

  assert(list->numentries > 0);
  // list->numentries--;
  __atomic_fetch_sub(&list->numentries, 1, __ATOMIC_RELEASE);

  page->next = NULL;
  page->prev = NULL;
  page->list = NULL;
  pthread_mutex_unlock(&(list->list_lock));
}

void next_page(struct fifo_list *list, struct tmem_page *page, struct tmem_page **next_page)
{   
    if (__atomic_load_n(&list->numentries, __ATOMIC_ACQUIRE) == 0) {
      *next_page = NULL;
      return;
    }
    pthread_mutex_lock(&(list->list_lock));
    if (page == NULL) {
        *next_page = list->last;
    }
    else {
        *next_page = page->prev;
        assert(page->list == list);
    }
    pthread_mutex_unlock(&(list->list_lock));
}
