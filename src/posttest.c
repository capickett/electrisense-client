#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>

#define FILENAME "test.file"
#define SERVER   "http://192.168.1.17:8080/"

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
  return size*nmemb; /* Do not print to stdout */
}

int main(int argc, char* argv[]) { 
  CURL* curl;
  CURLcode res;

  struct curl_httppost* formpost   = NULL;
  struct curl_httppost* lastptr    = NULL;
  struct curl_slist*    headerlist = NULL;
  static const char buf[] = "Expect:";

  int fd;
  struct stat fst;
  char* fbuf = NULL;

  int i;


  if ((fd = open(FILENAME, O_RDONLY)) < 0) {
    perror(FILENAME);
    return EXIT_FAILURE;
  }

  if (fstat(fd, &fst) < 0) {
    perror(FILENAME);
    return EXIT_FAILURE;
  }

  if ((fbuf = (char*) malloc(fst.st_size)) == NULL) {
    perror("malloc");
    return EXIT_FAILURE;
  }
  
  if (read(fd, (void*) fbuf, fst.st_size) < 0) {
    perror(FILENAME);
    return EXIT_FAILURE;
  }

  close(fd);

  curl_global_init(CURL_GLOBAL_ALL); /* Init curl vars */
  
  /* Add the file to the request */
  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "sendfile",
               CURLFORM_BUFFER, FILENAME,
               CURLFORM_BUFFERPTR, fbuf,
               CURLFORM_BUFFERLENGTH, fst.st_size,
               CURLFORM_END);
  
  for (i = 0; i < 10; i++) {
    printf("%d\n", i);
    curl = curl_easy_init(); /* Init an easy_session */
    headerlist = curl_slist_append(headerlist, buf);
    
    if (curl) {      
      /* Set the request uri */
      curl_easy_setopt(curl, CURLOPT_URL, SERVER);
      
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      
      /* Send the request */
      res = curl_easy_perform(curl);
      if (res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
      
      curl_easy_cleanup(curl);
    } else {
      fprintf(stderr, "curl: init failed\n");
    }
  }
  curl_slist_free_all(headerlist);
  curl_formfree(formpost);
  free(fbuf);
  return EXIT_SUCCESS;
}
