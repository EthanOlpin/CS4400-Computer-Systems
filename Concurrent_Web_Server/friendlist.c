/*
 * friendlist.c - [Starting code for] a web-based friend-graph manager.
 *
 * Based on:
 *  tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *      GET method to serve static and dynamic content.
 *   Tiny Web server
 *   Dave O'Hallaron
 *   Carnegie Mellon University
 */
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"

static void *doit(void * fd);
static dictionary_t *read_requesthdrs(rio_t *rp);
pthread_mutex_t lock;
static void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);
static void clienterror(int fd, char *cause, char *errnum, 
                        char *shortmsg, char *longmsg);
static void print_stringdictionary(dictionary_t *d);
//static void serve_request(int fd, dictionary_t *query);

static void serve_friends(int fd, dictionary_t *query);
static void do_befriend(int fd, dictionary_t *query);
static void do_unfriend(int fd, dictionary_t *query);
static void request_introduce(int fd, dictionary_t *query);
dictionary_t *friends;

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  friends = make_dictionary(1, (void (*)(void *))free_dictionary);
  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  /* Don't kill the server if there's an error, because
     we want to survive errors due to a client. But we
     do want to report errors. */
  exit_on_error(0);

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);
  pthread_mutex_init(&lock, NULL);
  while (1) {
    //TODO LAST: Create new thread for each new client?
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 0) {
      Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                  port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      pthread_t threadID;
      int *connfd_p = malloc(sizeof(int));
      *connfd_p = connfd;
      int err = pthread_create(&threadID, NULL, doit, (void*)connfd_p);
      if(err){
        printf("Thread creation error: %d\n", err);
      }
      else{
        pthread_detach(threadID);
      }
    }
  }
  
}

/*
 * doit - handle one HTTP request/response transaction
 */
void *doit(void * arg) {
  int fd = *((int *)arg);
  free(arg);
  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return NULL;
  printf("%s\n", buf);
  
  if (!parse_request_line(buf, &method, &uri, &version)) {
    clienterror(fd, method, "400", "Bad Request",
                "Friendlist did not recognize the request");
  } else {
    if (strcasecmp(version, "HTTP/1.0")
        && strcasecmp(version, "HTTP/1.1")) {
      clienterror(fd, version, "501", "Not Implemented",
                  "Friendlist does not implement that version");
    } else if (strcasecmp(method, "GET")
               && strcasecmp(method, "POST")) {
      clienterror(fd, method, "501", "Not Implemented",
                  "Friendlist does not implement that method");
    } else {
      headers = read_requesthdrs(&rio);

      /* Parse all query arguments into a dictionary */
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);
      if (!strcasecmp(method, "POST"))
        read_postquery(&rio, headers, query);

      /* For debugging, print the dictionary */
      print_stringdictionary(query);

      /* You'll want to handle different queries here,
         but the intial implementation always returns
         nothing: */
      if (starts_with("/friends?", uri))
        serve_friends(fd, query);
      else if (starts_with("/befriend?", uri))
        do_befriend(fd, query);
      else if (starts_with("/unfriend?", uri))
        do_unfriend(fd, query);
      else if (starts_with("/introduce?", uri))
        request_introduce(fd, query);
      /* Clean up */
      free_dictionary(query);
      free_dictionary(headers);
    }

    /* Clean up status line */
    free(method);
    free(uri);
    free(version);

  }
  Close(fd);
  return NULL;
}

/*
 * read_requesthdrs - read HTTP request headers
 */
dictionary_t *read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }
  printf("Header Dictionary: \n");
  print_stringdictionary(d);
  return d;
}

void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest) {
  char *len_str, *type, *buffer;
  int len;
  
  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");
  
  buffer = malloc(len+1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded")) {
    parse_query(buffer, dest);
  }

  free(buffer);
}

static char *ok_header(size_t len, const char *content_type) {
  char *len_str, *header;
  
  header = append_strings("HTTP/1.0 200 OK\r\n",
                          "Server: Friendlist Web Server\r\n",
                          "Connection: close\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n",
                          "Content-type: ", content_type, "\r\n\r\n",
                          NULL);
  free(len_str);

  return header;
}

/* Returns a newline separated string of the friends
 * belonging to the named user. Returns an empty string
 * if the user is a loser, and has no friends, or if user does not exist.
 */
static char* get_friends(char* name){
  pthread_mutex_lock(&lock);
  dictionary_t *friend_set = dictionary_get(friends, name);
  if(!friend_set){
    pthread_mutex_unlock(&lock);
    return strdup("");
  }
  int count = dictionary_count(friend_set);
  int i;
  char const** friend_arr = malloc(sizeof(size_t) * (count + 1));
  const char *friend;
  char *result;
  for(i = 0; i < count; i++){
    friend = dictionary_key(friend_set, i);
    friend_arr[i] = friend;
  }
  friend_arr[count] = NULL; //join_strings requires a NULL terminated array
  result = join_strings((const char * const*)friend_arr, '\n');
  free(friend_arr);
  pthread_mutex_unlock(&lock);
  return result;
}

/* Given the names of two users, add them to eachother's
 * collection of friends. If either of the two users have not 
 * been seen before, we create a new set for them.
 */
static void make_friends(char* n1, char* n2){
  //Names should not be identical
  if(!strcmp(n1, n2)){
    return;
  }
  pthread_mutex_lock(&lock);
  if(!dictionary_get(friends, n1))
    dictionary_set(friends, n1, make_dictionary(1, NULL));
  dictionary_set(dictionary_get(friends, n1), n2, NULL);

  if (!dictionary_get(friends, n2))
    dictionary_set(friends, n2, make_dictionary(1, NULL));
  dictionary_set(dictionary_get(friends, n2), n1, NULL);
  pthread_mutex_unlock(&lock);
}

/* Given the names of two users, remove them from eachother's
 * collection of friends. If either of the two users have no
 * friends after this operation, free their sets.
 */
static void unmake_friends(char *n1, char *n2)
{
  pthread_mutex_lock(&lock);
  dictionary_t *n1_friends = dictionary_get(friends, n1);
  dictionary_t *n2_friends = dictionary_get(friends, n2);
  if (n1_friends) {
    dictionary_remove(n1_friends, n2);
    if (dictionary_count(n1_friends) == 0) {
      dictionary_remove(friends, n1);
    }
  }
  if (n2_friends) {
    dictionary_remove(n2_friends, n1);
    if (dictionary_count(n2_friends) == 0) {
      dictionary_remove(friends, n2);
    }
  }
  pthread_mutex_unlock(&lock);
}

/*
 * serve_freinds - Returns the friends of ‹user›, each on
 * a separate newline-terminated line as plain text 
 * (i.e., text/plain; charset=utf-8).  The result is empty
 * if no friends have been registered for the user.
 */
static void serve_friends(int fd, dictionary_t *query){
  size_t len;
  char *body, *header;
  char *user = dictionary_get(query, "user");
  body = get_friends(user);
  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

/* Adds each user in ‹friends› as a friend of ‹user›, which
 * implies adding ‹user› as a friend of each user in ‹friends›.
 * The ‹friends› list can be a single user or multiple
 * newline-separated user names, and ‹friends› can optionally
 * end with a newline character.
 */
static void do_befriend(int fd, dictionary_t *query){
  size_t len;
  char *body, *header;
  char *user = dictionary_get(query, "user");
  char **new_friends = split_string(dictionary_get(query, "friends"), '\n');
  char *curr = new_friends[0];
  int i = 0;
  while(curr){
    make_friends(curr, user);
    free(curr);
    curr = new_friends[++i];
  }
  free(new_friends);
  body = get_friends(user);
  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
}

/* Removes each user in ‹friends› as a friend of ‹user› and vice versa.
 * The result should be a list of remaining friends to ‹user›.
 */
static void do_unfriend(int fd, dictionary_t *query){
  size_t len;
  char *body, *header;
  char *user = dictionary_get(query, "user");
  char **unfriends = split_string(dictionary_get(query, "friends"), '\n');
  char *curr = unfriends[0];
  int i = 0;
  while (curr)
  {
    unmake_friends(curr, user);
    free(curr);
    curr = unfriends[++i];
  }
  free(unfriends);
  body = get_friends(user);
  len = strlen(body);

  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);

  free(header);

  /* Send response body to client */
  Rio_writen(fd, body, len);

  free(body);
  return;
}

/* The server behaves like a web-client and makes a request to the server
 * that corresponds to clientfd. Stores the response content (if there is
 * any) in content, and returns the content length.
 */

static int make_request(int clientfd, char** content, char* request){
  int request_size = strlen(request);
  size_t content_length = 0;
  rio_t rio;
  char buf[MAXLINE], *status;
  Rio_writen(clientfd, request, request_size);
  Rio_readinitb(&rio, clientfd);
  dictionary_t *headers = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(&rio, buf, MAXLINE);
  parse_status_line(buf, NULL, &status, NULL);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, headers);
  }
  printf("Header Dictionary: \n");
  print_stringdictionary(headers);

  if (strcmp(status, "200"))
  {
    printf("Server responded with status %s", status);
  }
  else
  {
    content_length = atoi(dictionary_get(headers, "Content-length"));
    if (content_length)
    {
      *content = malloc(content_length);
      Rio_readnb(&rio, *content, content_length);
    }
  }
  free(status);
  free_dictionary(headers);
  return content_length;
}

/* Contacts a friend-list server running on ‹host› at ‹port› to get all
 * of the friends of ‹friend›, and adds ‹friend› plus all of ‹friend›’s
 *  friends as friends of ‹user› and vice versa.
 */
static void request_introduce(int fd, dictionary_t *query){
  char *host = dictionary_get(query, "host");
  char *port = dictionary_get(query, "port");
  char *user = dictionary_get(query, "user");
  char *friend = query_encode(dictionary_get(query, "friend"));

  int clientfd = Open_clientfd(host, port);
  char* request = append_strings("GET /friends?user=", friend, " HTTP/1.1\r\n\r\n", NULL);
  char* content;
  size_t content_length;
  content_length = make_request(clientfd, &content, request);
  make_friends(user, friend);
  if(content_length){
      char **new_friends = split_string(content, '\n');
      char *curr = new_friends[0];
      int i = 0;
      int bytes_read = 0;
      while (bytes_read < content_length)
      {
        //Plus 1 for newline character ommitted by split_string.
        bytes_read += strlen(curr) + 1;
        make_friends(curr, user);
        free(curr);
        curr = new_friends[++i];
      }
      free(content);
      free(new_friends);
  }
  //Send OK response to client to let them know we processed the
  //request
  char* header = ok_header(0, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  free(request);
  free(header);
  free(friend);
  return;
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) {
  size_t len;
  char *header, *body, *len_str;

  body = append_strings("<html><title>Friendlist Error</title>",
                        "<body bgcolor=""ffffff"">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>Friendlist Server</em>\r\n",
                        NULL);
  len = strlen(body);

  /* Print the HTTP response */
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg, "\r\n",
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);
  
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  free(header);
  free(body);
}

static void print_stringdictionary(dictionary_t *d) {
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++) {
    printf("%s=%s\n",
           dictionary_key(d, i),
           (const char *)dictionary_value(d, i));
  }
  printf("\n");
}
