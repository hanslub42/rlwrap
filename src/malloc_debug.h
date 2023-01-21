

#ifdef DEBUG
  void *debug_malloc(size_t size, char *file, int line);
  void debug_free(void *ptr, char *file, int line);
  void debug_postmortem(void);
  void free_foreign(void *ptr);
  void *malloc_foreign(size_t size);
  void *copy_and_free_for_malloc_debug(void *ptr, size_t size);
  char *copy_and_free_string_for_malloc_debug(char* str);


  #define free(ptr) debug_free(ptr,__FILE__,__LINE__)
  #define mymalloc(size) debug_malloc(size, __FILE__, __LINE__)
#else
  #define free_foreign free
  #define malloc_foreign malloc
  #define copy_and_free_for_malloc_debug(ptr,size) (ptr)
  #define copy_and_free_string_for_malloc_debug(str) (str)
  #define debug_postmortem() do_nothing(0)


#endif
