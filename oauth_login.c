
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <oauth.h>

/**
 * split and parse URL parameters replied by the test-server
 * into <em>oauth_token</em> and <em>oauth_token_secret</em>.
 */
int parse_reply(const char *reply, char **token, char **secret) {
  int rc;
  int ok=1;
  char **rv = NULL;
  rc = oauth_split_url_parameters(reply, &rv);
  qsort(rv, rc, sizeof(char *), oauth_cmpstringp);
  if( rc==2 
      && !strncmp(rv[0],"oauth_token=",11)
      && !strncmp(rv[1],"oauth_token_secret=",18) ) {
    ok=0;
    if (token)  *token =strdup(&(rv[0][12]));
    if (secret) *secret=strdup(&(rv[1][19]));
    printf("key:    '%s'\nsecret: '%s'\n",*token, *secret); // XXX token&secret may be NULL.
  }
  if(rv) free(rv);
  return ok;
}

/** 
 * an example requesting a request-token from an OAuth service-provider
 * exchaning it with an access token
 * and make an example request.
 * exercising either the oauth-HTTP GET or POST function.
 */
int oauth_consumer_example(int use_post) 
{
  const char *test_call_uri = "http://restful.airplayme.com/people/loved/artists.json";
  // for airplay 
  const char *c_key         = "c6ff8d1fc00b81c5f79595f8f79068d2"; //< consumer key
  const char *c_secret      = "10260d0ed691e8b16c3d2733"; //< consumer secret
  
  const char *t_key = "2de4051ac64e7f86c3c5f141bfa1f643";
  const char *t_secret ="8d158b552ae6efbc93c899c0";

   char *req_url = NULL;
   char *postarg = NULL;
   char *reply   = NULL;
#if 0
// post  loved songs
    const char *test_add_people =  "http://restful.airplayme.com/people/loved/artists.json?id=araba99854f32511dfb8bf5465fbbdc18f";
    
    req_url = oauth_sign_url2(test_add_people, &postarg, OA_HMAC, NULL, c_key, c_secret, t_key, t_secret);
    printf("*** POST req_url=:'%s'\n",req_url);    
    reply = oauth_http_post(req_url,postarg);
    printf("reply=:'%s'\n",reply);    
#endif  
    printf("make some request..\n");
    if(req_url) 
    {
        free(req_url);
        req_url = NULL;
    }
    if(postarg) 
    {
        free(postarg);
        postarg = NULL;
    }
    postarg = NULL;
    if (use_post) 
    {
        req_url = oauth_sign_url2(test_call_uri, &postarg, OA_HMAC, NULL, c_key, c_secret, t_key, t_secret);
        reply = oauth_http_post(req_url,postarg);
    } 
    else 
    {
        req_url = oauth_sign_url2(test_call_uri, NULL, OA_HMAC, NULL, c_key, c_secret, t_key, t_secret);
        printf("query:'%s'\n",req_url);
        printf("***   postarg=:'%s'\n", postarg);    
        reply = oauth_http_get(req_url,postarg);
        printf("reply=:'%s'\n",reply);    
    }  
    if(req_url) 
        free(req_url);
    if(postarg) 
        free(postarg);

    return(0);
}

int oauth_sina_example(int use_post) {
    const char *test_call_uri = "http://api.t.sina.com.cn/users/show/1602088093.json?source=4274370472";
    const char *c_key = "b94af33dc59e712f9eebed78eea1e218";
    const char *c_secret = "d3531bb5b69f8ddc962ac6faa4aa9db7";
  
    const char *t_key = "b94af33dc59e712f9eebed78eea1e218";
    const char *t_secret ="d3531bb5b69f8ddc962ac6faa4aa9db7";
    char *req_url = NULL;
    char *postarg = NULL;
    char *reply   = NULL;
 
    printf("*** Get from sina:...\n");
    if (use_post)
    {
        req_url = oauth_sign_url2(test_call_uri, &postarg, OA_HMAC, NULL, c_key, c_secret, t_key, t_secret);
        reply = oauth_http_post(req_url,postarg);
    } 
    else 
    {
        req_url = oauth_sign_url2(test_call_uri, NULL, OA_HMAC, NULL, c_key, c_secret, t_key, t_secret);
        printf("Access Sina query:'%s'\n",req_url);
        reply = oauth_http_get(req_url,postarg);
        printf("Get reply=:'%s'\n",reply);    
    }  
    if(req_url) 
        free(req_url);
    if(postarg) 
        free(postarg);

    return(0);
}

/**
 * Main Test and Example Code.
 * 
 * compile:
 *  gcc -lssl -loauth -o oauthexample oauthexample.c
 */

int main (int argc, char **argv) {

oauth_sina_example(1);

  switch(oauth_consumer_example(0)) {
    case 1:
      printf("HTTP request for an oauth request-token failed.\n");
      break;
    case 2:
      printf("did not receive a request-token.\n");
      break;
    case 3:
      printf("HTTP request for an oauth access-token failed.\n");
      break;
    case 4:
      printf("did not receive an access-token.\n");
      break;
    case 5:
      printf("test call 'echo-api' did not respond correctly.\n");
      break;
    default:
      printf("request ok.\n");
      break;
  }
  return(0);
}
