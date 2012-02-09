#include <iostream>
//#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lib_json_0.5/json.h"
#include <malloc.h>

#include "data_manager.h"

#include <sys/ioctl.h>
#include  <net/if.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <sys/stat.h>

#include <pthread.h> 

DataManager *DataManager::m_instance = NULL;

CURL * DataManager::mCurl = NULL;
CURL * DataManager::pLogoCurl = NULL;

SongInfo  * DataManager::pDownList=NULL;

unsigned int DataManager::RadioIndex =0;
// for on line parse info
unsigned int DataManager::retSongNum =0;
unsigned int DataManager::retAlbumNum=0;
unsigned int DataManager::retMyRadioNum=0;
unsigned int DataManager::retArtistNum=0;

bool DataManager::start_dl_flag=false;
bool DataManager::restart_dl_flag=false;

static const char radio_id[2][TRACK_ID_BUFLEN] = { {"rd4dbd928e4220cf367c000001"}, 
                                                   {"rd4dbd928e4220cf367c000002"}};
static const char *prefix = "/tmp/cache/";

static char tracks_list[TRACK_MXA_NUM][TRACK_ID_BUFLEN];


//#define DEBUG_API
#ifdef DEBUG_API
#define dbg_printf(fmt...)   printf(fmt...);
#else
#define dbg_printf(fmt) 
#endif

static void get_last_name(const char *pUrl, char *pName)
{
    const char *pSlash;
    const char *pDot;
    int j=0;    
    if(!pUrl || !pName)
        return;

    pSlash = strrchr(pUrl, '/');
    if(!pSlash)
        return;
            
    pDot = strrchr(pSlash, '.');
    if(!pDot)
        return;        
    
    while(pSlash != pDot) {
        pSlash++;
        if(pSlash == pDot)
            break;
            pName[j] = *pSlash;
            j++;
    }
    pName[j++] = '\0';
}

static void songs_init(SongInfo *pSongs, unsigned int size)
{
    unsigned int i;    
    for(i=0; i<size; i++) {
        pSongs->name = (char *) malloc(1);
        pSongs->location= (char *) malloc(1);
        pSongs->album_logo =(char *) malloc(1);
        pSongs->lyric=(char *) malloc(1);
        pSongs->singers=(char *) malloc(1);
        pSongs->album_title=(char *) malloc(1);
        pSongs++;
    }
}

static void songs_free(SongInfo *pSongs, unsigned int size)
{
    unsigned int i;    
    for(i=0; i<size; i++){        
        free((void *)pSongs->name);
        free((void *)pSongs->location);
        free((void *)pSongs->album_logo);
        free((void *)pSongs->lyric);
        free((void *)pSongs->album_title);
        pSongs++;
    }
}

static void albums_init(pAlbumInfo pAlbums, unsigned int size)
{
    unsigned int i;    
    for(i=0; i<size; i++){        
        pAlbums++;
    }
}

static void albums_free(pAlbumInfo pAlbums, unsigned int size)
{
    unsigned int i;    
    for(i=0; i<size; i++){        
        //free((void *)pAlbums->album_logo);
        //free((void *)pAlbums->album_title);
        pAlbums++;
    }
}

DataManager::DataManager():mLogged(false)
{
    curl_global_init(CURL_GLOBAL_ALL);
    mCurl = curl_easy_init();
    pLogoCurl = curl_easy_init();
    start_dl_flag = false;

    user_id = NULL;
    password =NULL;

    songs_init(SongRndList, XIAMI_MAX_RND_NUM);
    songs_init(MySongs, XIAMI_MY_SONGS_PAGE);
    songs_init(apMyAlbums, XIAMI_MY_SONGS_PAGE);
    songs_init(MyArtists, XIAMI_MY_SONGS_PAGE);
    songs_init(MyRadios, XIAMI_MY_SONGS_PAGE);

    albums_init(albums_list, XIAMI_MY_SONGS_PAGE);
    
#ifdef LYRIC_DL_SUPPORT    
    // check the download directory, 
    struct stat st;
    if(stat(prefix, &st) != 0)
    {
        int status;
        status = mkdir(prefix, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if(status != 0)
            printf("mkdir failed\n");
    }
    // create a thread to download the lyric and logo
    pthread_t  dl_thread;       
    pthread_create(&dl_thread,  NULL,  &(DataManager::do_dl_thread), NULL);
#endif    
 }

DataManager::~DataManager()
{    
    curl_easy_cleanup(mCurl);
    curl_easy_cleanup(pLogoCurl);
    
    songs_free(SongRndList, XIAMI_MAX_RND_NUM);
    
    songs_free(MySongs, XIAMI_MY_SONGS_PAGE);
    songs_free(apMyAlbums, XIAMI_MY_SONGS_PAGE);
    songs_free(MyRadios, XIAMI_MY_SONGS_PAGE);
    songs_free(MyArtists, XIAMI_MY_SONGS_PAGE);

    albums_free(albums_list, XIAMI_MY_SONGS_PAGE);
    
    free(user_id);
    free(password);    
}

DataManager *DataManager::getInstance()
{
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex);
    if ( m_instance == NULL ) {
        m_instance = new DataManager;
    }
    pthread_mutex_unlock(&mutex);
    return m_instance;
}

void DataManager::releaseInstance()
{
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex);
    delete m_instance;
    pthread_mutex_unlock(&mutex);
}

size_t  DataManager::write_file(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written;
    
    written = fwrite(ptr, size, nmemb, stream);    
    return written;
}

 size_t DataManager::WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	
  	struct MemoryStruct *mem = (struct MemoryStruct *)data;
    
  	mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
    		/* out of memory! */ 
    		printf("not enough memory (realloc returned NULL)\n");   
		return 0;
	}
	  memcpy(&(mem->memory[mem->size]), ptr, realsize);
	  mem->size += realsize;
	  mem->memory[mem->size] = 0;
	 
	  return realsize;
}

//@return 0 --- sucessful, -1 ---  failed get ip address, -2 --- ping failed
int DataManager::ping_xiami(const char *ifname)
{
    struct ifreq s;
    int ret = -1;
    
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    strcpy(s.ifr_name, ifname);
    ret = ioctl(fd, SIOCGIFADDR, &s);
    if (0 == ret){
        printf("%s\n", inet_ntoa(((struct sockaddr_in *)&s.ifr_addr)->sin_addr));
       ret=system("ping -c 1 www.xiami.com"); // ping xiami website
       if( 0 == ret)
            return 0;
       else
            return -2;
    }
    return -1;
}
#define _CURL_DEBUG
// a general function to get data from a URL
// @pUrl ---  to connect url
// @id    ---- your accout's ID from the login return, if not required, fill 0
bool DataManager::GetDataFromUrl(const char *pUrl, char **pBuf)
{
    int ret = -1;
    CURLcode code;
    struct MemoryStruct chunk;

    chunk.memory =(char *) malloc(1);  /* will be grown as needed by the realloc above */ 
    chunk.size = 0;    

    char user_agent[]="Mozilla/5.0 (Windows NT 5.1) AppleWebKit/534.24 (KHTML, like Gecko) Chrome/11.0.696.50 Safari/534.24";

#ifdef _CURL_DEBUG
    char error[1024]={0};
    memset(error, 0, 1024);

    curl_easy_setopt(mCurl, CURLOPT_VERBOSE, AP_CURLOPT_VERBOSE_DEBUG);
    curl_easy_setopt(mCurl, CURLOPT_ERRORBUFFER, error);
#endif

#ifdef _CURL_BASIC_AUTH
    char user[128] ={0};
    memset(user, 0, 128);

    if(mUserID == 0) {
        printf("Please login before do collect\n");
        return false;
     }
    memcpy(user, user_id,  strlen(user_id));
    memcpy(user+strlen(user_id), ":", 1);
    memcpy(user+strlen(user_id)+1, password, strlen(password));
        
    printf("**** user=%s, user_id=%s, pwd=%s\n", user, user_id, password);
    curl_easy_setopt(mCurl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(mCurl, CURLOPT_USERPWD, user);
#endif 

    //set user_agent
    code=curl_easy_setopt(mCurl, CURLOPT_USERAGENT, user_agent);
    if(code != CURLE_OK){
        printf("set opt user agent error=%s\n", error);
        return false;
    }
    ret = curl_easy_setopt(mCurl, CURLOPT_URL, pUrl);
    if(0 != ret)
    {
        printf("set opt url error\n");
        return false;
    }    
    /* send all data to this function  */ 
    curl_easy_setopt(mCurl, CURLOPT_WRITEFUNCTION, &DataManager::WriteMemoryCallback); 
	
    /* we pass our 'chunk' struct to the callback function */ 
    curl_easy_setopt(mCurl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(mCurl, CURLOPT_TIMEOUT, HTTP_CONNECTION_TIMEOUT);
    ret = curl_easy_perform(mCurl);
    if (ret  != CURLE_OK)
    {
#ifdef _CURL_DEBUG
        printf("perform error:%s\n", error);
#else        
        printf("perform error\n");
#endif        
        return false;
    }
    (*pBuf) = chunk.memory;
    return true;
}

// example: successful return
//  status: "ok"
//  user_id: "4460056"
//  nick_name: "shengdatest"
//  url: "122.225.11.135"
//  client_port: 4670
int  DataManager::ParseLoginInfo(struct MemoryStruct *pJson)
{
    printf("*** Longin-->>");
    return  true;
}
////////////////// for airplay interface 
static bool apParseErrCode(const char *buf, Json::Value &root)
{
	Json::Reader reader;
	bool ret;
    
	ret = reader.parse(buf, root);
	 if (!ret)
	{
	       // report to the user the failure and their locations in the document.
		std::cout  << "Failed to parse configuration\n"<< reader.getFormatedErrorMessages();
		return  false;
	 }
	if (root["err_code"].asInt())
	{
		printf("get songs info error\n");
		return false;
	}    
        return true;
}

static void fillValue(const char **pBuf, const char *pValue, int size)
{
        *pBuf = (const char *) realloc((void *)*pBuf, size +1);
        memset((void *)*pBuf, 0, size+1);
        memcpy((void *)*pBuf, (const void *)pValue, size);
}

int DataManager:: apParseRadioTracks(char *pBuf)
{
	Json::Reader reader;
	Json::Value value;
	bool ret;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    
    int track_size = 0;   
    Json::Value tracks = value["tracks"];
    track_size   = tracks.size();

    printf(" track size=%d\n", track_size);
    std::string  track_id;
    int i =0;
    for(i=0; i < track_size && track_size <=TRACK_MXA_NUM; i++) 
    {
        strncpy(tracks_list[i], tracks[i].asString().c_str(), TRACK_ID_LEN);
        tracks_list[i][TRACK_ID_LEN]='\0';
    }
    return true;
}

int DataManager::apParseSongBasic(char *pBuf, SongInfo *pSong)
{
    Json::Value value;
    bool ret;
    int size;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    
    size = value["name"].asString().size();
    pSong->name = (const char *) realloc((void *)pSong->name, size +1);
    memset((void *)pSong->name, 0, size+1);
    memcpy((void *)pSong->name, (const void *)value["name"].asString().c_str(), size);
        
    Json::Value artist;
    Json::Value album;

    artist = value["artist"];
    unsigned int i=0;
    if(artist[i]["name"].isNull())
    {
        pSong->singers= (const char *) realloc((void *)pSong->singers, 7);
        sprintf((char *)pSong->singers, "Unkonw");
    }else {
        size = artist[i]["name"].asString().size();
        pSong->singers= (const char *) realloc((void *)pSong->singers, size +1);
        memset((void *)pSong->singers, 0, size+1);
        memcpy((void *)pSong->singers, (const void *)artist[i]["name"].asString().c_str(), size);
    }
    
    album = value["album"];
    if(album["name"].isNull())
    {
        pSong->album_title= (const char *) realloc((void *)pSong->album_title, 7);
        memset((void *)pSong->album_title, 0, 7);
        memcpy((void *)pSong->album_title, "Unkonw", 6);    
    }else {
        size = album["name"].asString().size();
        pSong->album_title= (const char *) realloc((void *)pSong->album_title, size +1);
        memset((void *)pSong->album_title, 0, size+1);
        memcpy((void *)pSong->album_title, (const void *)album["name"].asString().c_str(), size);
   }
    return true;
}

int DataManager::apParseSongPlay(char *pBuf, SongInfo *pSong)
{
    Json::Value value;
    bool ret;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    
    Json::Value url;
    Json::Value lrc;
    unsigned int i=0, j=2;
    
    url = value["url"];    
    lrc = value["lrc"];
    if(url[i].isNull()){
        fillValue(&pSong->location, "Unkonw", 6);
    }
    else
        fillValue(&pSong->location, url[i].asString().c_str(), url[i].asString().size());
    
    if(lrc[i][j].isNull()){
        fillValue(&pSong->lyric, "Unkonw", 6);
    }else  
        fillValue(&pSong->lyric,  lrc[i][j].asString().c_str(), lrc[i][j].asString().size());
    
    return true;
}

int DataManager::apParseLovedSongs(char *pBuf, SongInfo *pSong)
{
    Json::Value value;
    bool ret;
    int size;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    
    Json::Value entries;
    Json::Value artist;
    Json::Value album;
    
    int i=0, j=0;
    
    entries = value["entries"];
    size = entries.size();
    retSongNum = size;
    
    printf("apParseLovedSongs:: entries size=%d\n", size);
    for(i=0; i<size && i<XIAMI_MY_SONGS_PAGE; i++)
    {
            strncpy(pSong->track_id, entries[i]["id"].asString().c_str(), 34);
            pSong->track_id[35] = '\0';
            fillValue(&pSong->name, entries[i]["name"].asString().c_str(), entries[i]["name"].asString().size());
            
            artist = entries[i]["artist"];
            if(artist.isNull())
                fillValue(&pSong->singers, "Unknow", 6);
            else
                fillValue(&pSong->singers, artist[j]["name"].asString().c_str(), artist[j]["name"].asString().size());
            
            album = entries[i]["album"];
            if(album["name"].isNull() )
                fillValue(&pSong->album_title, "unKnow", 6);
            else
                fillValue(&pSong->album_title,  album["name"].asString().c_str(), album["name"].asString().size());
            
            pSong++;
   }
    return true;
}

int DataManager::apParseLovedArtists(char *pBuf, SongInfo *pSong)
{
    Json::Value value;
    bool ret;
    int size;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    
    Json::Value entries;    
    int i=0;
    
    entries = value["entries"];
    size = entries.size();
    retArtistNum= size;
    
    printf("apParseLovedSongs:: entries size=%d\n", size);
    for(i=0; i<size && i<XIAMI_MY_SONGS_PAGE; i++) 
   {
        strncpy(pSong->track_id, entries[i]["id"].asString().c_str(), 34);
        pSong->track_id[35] = '\0';
        fillValue(&pSong->singers, entries[i]["name"].asString().c_str(), entries[i]["name"].asString().size());
        pSong++;
    }
    return true;
}

int DataManager::apParseLovedAlbums(char *pBuf, SongInfo *pSong)
{
    Json::Value value;
    bool ret;
    int size;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    
    Json::Value entries;    
    int i=0;
    
    entries = value["entries"];
    size = entries.size();
    retAlbumNum= size;
    
    printf("apParseLovedSongs:: entries size=%d\n", size);
    for(i=0; i<size && i<XIAMI_MY_SONGS_PAGE; i++) 
   {
        strncpy(pSong->track_id, entries[i]["id"].asString().c_str(), 34);
        pSong->track_id[35] = '\0';
        fillValue(&pSong->album_title, entries[i]["name"].asString().c_str(), entries[i]["name"].asString().size());
        pSong++;
    }
    return true;
}

int DataManager::apParseLovedRadios(char *pBuf, SongInfo *pSong)
{
    Json::Value value;
    bool ret;
    int size;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    
    Json::Value entries;    
    int i=0;
    
    entries = value["entries"];
    size = entries.size();
    retMyRadioNum = size;
    
    printf("apParseLovedRadios:: entries size=%d\n", size);
    for(i=0; i<size && i<XIAMI_MY_SONGS_PAGE; i++) 
   {
        strncpy(pSong->track_id, entries[i]["id"].asString().c_str(), 34);
        pSong->track_id[35] = '\0';
        fillValue(&pSong->album_title, entries[i]["name"].asString().c_str(), entries[i]["name"].asString().size());
        pSong++;
    }
    return true;
}

pAlbumInfo DataManager::apParseArtistAlbums(char *pBuf, unsigned int &size)
{
    Json::Value value;
    bool ret;
    
    size = 0;
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return (pAlbumInfo)NULL;
    
    Json::Value works;
    Json::Value album;
    unsigned int i;

    works = value["works"];
    album = works["album"];
    size = album.size();
    
    pAlbumInfo pAlbum, pHeadAlbum;
    
    pHeadAlbum = new AlbumInfo [size];
    pAlbum = pHeadAlbum;

    int len;
    for(i=0; i < size; i++)
   {
        strncpy(pAlbum->album_id, album[i]["id"].asString().c_str(), 34);
        pAlbum->album_id[35] = '\0';
        
        len = album[i]["name"].asString().size();
        //pAlbum->album_title = new char [len +1];
        if(len > 127) // ignor  the long name
            len = 127;
        strncpy(pAlbum->album_title,  album[i]["name"].asString().c_str(), len);
        pAlbum->album_title[len]='\0';
        pAlbum++;
    }
    return pHeadAlbum;
}

bool DataManager::apParseAlbumImages(char *pBuf, char *pImages)
{
    Json::Value value;
    bool ret;
    int len;
    
    ret = apParseErrCode((const char*)pBuf, value);
    if(ret == false)
        return false;
    len = value["m"].asString().size();
    if(len > 127)
        len = 127;
    strncpy(pImages, value["m"].asString().c_str(), len);
    pImages[len]= '\0';
    
    return true;
}

bool DataManager::apGetSongBasic( const char *pTrackID, SongInfo *pSong)
{
    const char ap_song_basic_url[] = "http://restful.airplayme.com/tracks/%s.json";
    char *url;
    bool ret;
    char *pBuf;
    
    url = new char [256];
    sprintf(url, ap_song_basic_url, pTrackID);
    ret = GetDataFromUrl(url,  &pBuf);
    if(ret)
        ret = apParseSongBasic(pBuf, pSong);
        
    delete url;
    free((void *)pBuf);
    
    return ret;
}

int DataManager:: apGetSongUrl( const char *pTrackID, SongInfo *pSong)
{
    const char *songTrackUrl = "http://restful.airplayme.com/tracks/%s/play.json?type=mp3";
    char *url;
    bool ret;
    char *pBuf;
    
    url = new char[256];
    sprintf(url, songTrackUrl, pTrackID);
    ret = GetDataFromUrl(url, &pBuf);
    if(ret)
        ret = apParseSongPlay(pBuf, pSong);
    
    delete url;
    return ret;
}

bool DataManager::apGetAlbumLogo(const char *pAlbumId, char * pAlbumImage)
{
    const char *AlbumImageUrl = "http://restful.airplayme.com/albums/%s/images.json";
    char url[128];
    bool ret;
    char *pBuf;
    
    sprintf(url, AlbumImageUrl, pAlbumId);
    ret = GetDataFromUrl(url, &pBuf);
    if(ret) 
    {
        ret = apParseAlbumImages(pBuf, pAlbumImage);
    }    
    return ret;
}
// endof airplay 

bool DataManager::downloadSong( const char *pFileName,  const char *pUrl, int file_type)
{	
    FILE *fp;
    int ret = 0;
    int size;
    
    char postfix[4] = {0};
    
    if(!pFileName || !pUrl )
    {
        printf("Null pFilename or pUrl\n");
        return false;
    }
    
    switch(file_type)
    {
        case FILE_TYPE_MP3:
            strcpy(postfix, ".mp3");
            break;
         case FILE_TYPE_LOGO:
            strcpy(postfix, ".jpg");
            break;
         case FILE_TYPE_LYRIC:
            strcpy(postfix, ".lrc");
            break;
         default:
             strcpy(postfix, ".mp3");
            break;
    }
    size = strlen(prefix) + strlen(pFileName) +5;
    char path_name[256] = {0}; 

    memset(path_name, 0, size);
    memcpy(path_name, prefix, strlen(prefix));
    memcpy(path_name+strlen(prefix), pFileName, strlen(pFileName));    
    memcpy(path_name+strlen(prefix)+strlen(pFileName), postfix, sizeof(postfix));
    path_name[size]='\0';    
    printf("download :: pFileNmae=%s, name=%s, size=%d\n", pFileName, path_name, size);
    if(access(path_name, F_OK) == 0)
    {
            printf(" file exist return \n");
            return true;
    }
    fp = fopen(path_name, "wb");
    if(NULL == fp)
    {
        printf("open file error\n");
        return false;
    }
    if (mCurl) {
        curl_easy_setopt(pLogoCurl, CURLOPT_URL, pUrl);
        curl_easy_setopt(pLogoCurl, CURLOPT_WRITEFUNCTION, &DataManager::write_file);
        curl_easy_setopt(pLogoCurl, CURLOPT_WRITEDATA, fp);        
        //curl_easy_setopt(mCurl, CURLOPT_NOPROGRESS, FALSE);        
	// Install the callback function
	//curl_easy_setopt(mCurl, CURLOPT_PROGRESSFUNCTION, progress_func);
        ret = curl_easy_perform(pLogoCurl);
        if(ret != CURLE_OK)
        {
            fclose(fp);
            return false;
        }
    }
     fclose(fp);
    return true;
}

bool DataManager::downUrl( const char *pFileName,  const char *pUrl)
{	
    FILE *fp;
    CURLcode  ret;
    int size;
    
    if(!pFileName || !pUrl )
    {
        printf("Null pFilename or pUrl\n");
        return false;
    }
    size = strlen(prefix);
    char path_name[256] = {0}; 
    memcpy(path_name, prefix, strlen(prefix));
    memcpy(path_name+size, pFileName, strlen(pFileName));    
    size += strlen(pFileName);
    path_name[size]='\0';    
    printf("download :: pFileNmae=%s, name=%s, size=%d\n", pFileName, path_name, size);
    
    if(access(path_name, F_OK) == 0)
    {
            printf(" file exist return \n");
            return true;
    }
    fp = fopen(path_name, "wb");
    if(NULL == fp)
    {
        printf("open file error\n");
        return false;
    }
    if (mCurl) {
        curl_easy_setopt(pLogoCurl, CURLOPT_URL, pUrl);
        curl_easy_setopt(pLogoCurl, CURLOPT_WRITEFUNCTION, &DataManager::write_file);
        curl_easy_setopt(pLogoCurl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(mCurl, CURLOPT_TIMEOUT, HTTP_CONNECTION_TIMEOUT);    
        ret = curl_easy_perform(pLogoCurl);
        if(ret != CURLE_OK)
        {
            fclose(fp);
            return false;
        }
    }
     fclose(fp);
    return true;
}

 void *DataManager::do_dl_thread( void * parameter)
{
    int i;
    char buf[128];

    pthread_detach(pthread_self());
    
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex,NULL);
    
    SongInfo *pSongs = SongRndList;
    int size = 0;
    while(1) {
        if(start_dl_flag && pDownList) {
            pSongs = pDownList;
            size = pSongs->size;
                for(i=0; i<size; i++) {
                        if(restart_dl_flag)
                            break;
                        get_last_name(pSongs->location, buf);
                        if(pSongs->album_logo)
                            downloadSong(buf, pSongs->album_logo, FILE_TYPE_LOGO);
                        if(strlen(pSongs->lyric) > 6)
                             downloadSong(buf, pSongs->lyric, FILE_TYPE_LYRIC);
                        pSongs++;
                  }
                pthread_mutex_lock(&mutex);
                if(i == size) {
                    // turn off dl flag, finished
                    start_dl_flag = false;
                }else {
                    printf("*** restart dl list\n");
                    restart_dl_flag = false; // already update the dl list, restart 
                }
                pthread_mutex_unlock(&mutex);             
        }
        sleep(1);
    }
    return NULL;
}

bool DataManager::GetFavoriteInfo(const int pageNum, int type)
{
    const char uid[]="ps1748659561";
    
    const char mySongs[] = "http://restful.airplayme.com/people/%s/loved/tracks.json";
    const char myArtists[]="http://restful.airplayme.com/people/%s/loved/artists.json";
        
    const char myAlbums[]="http://restful.airplayme.com/people/%s/collected/albums.json";
    const char myRadios[]="http://restful.airplayme.com/people/%s/collected/radios.json";
    
    char url[256];
    char *pBuf;
    bool ret;
    
    memset(url, 0, 256);
    switch(type)
    {
        case MY_SOGNS:
            sprintf(url, mySongs, uid);
            GetDataFromUrl(url, &pBuf);
            ret=apParseLovedSongs(pBuf, MySongs);
            break;
        case MY_ALBUMS:
            sprintf(url, myAlbums, uid);            
            GetDataFromUrl(url, &pBuf);
            apParseLovedAlbums(pBuf, apMyAlbums);
            break;
         case MY_ARTISTS:
            sprintf(url, myArtists, uid);                        
            GetDataFromUrl(url, &pBuf);
            apParseLovedArtists(pBuf, MyArtists);            
            break;
        case MY_RADIOS:
            sprintf(url, myRadios, uid);
            GetDataFromUrl(url, &pBuf);
            apParseLovedRadios(pBuf, MyRadios);            
            break;
        default:
            return false;
    }
    
    if(pBuf)
        free((void *)pBuf);
    
    return ret;
}

//////////////////  action API functions
bool DataManager::Login(const char *account, const char *passwd)
{
//    const char URLFormat[] = "http://api.xiami.com/app/android/login?email=%s&pwd=%s";
    return true;
}

////////////////// Upper API for UI
SongInfo * DataManager:: get_rnd_list(int &result)
{
    bool rnd;
    static int cnt = 0; 
    //const char *ap_rnd_url = "http://api.airplayme.com/0.4/radio/turn_on/rd4dbd928e4220cf367c000011.json";
    const char *ap_rnd_url = "http://restful.airplayme.com/radios/rd4dbd928e4220cf367c000011/play.json";
    char *pReadBuf;
    
    result = 0;
//check wether the random track list is blank, otherwise update the track list
    //if(cnt == 0)
    //{
        rnd = GetDataFromUrl(ap_rnd_url, &pReadBuf);        
        rnd = apParseRadioTracks(pReadBuf);
        free((void *)pReadBuf);
        
        if(rnd == false)
        {
            printf("get_rnd_list get ---data from url failed\n");
            return (SongInfo *)NULL;
        }
//parse the track's song info, song name, etc...
        int i;
        cnt +=XIAMI_MAX_RND_NUM;
        SongInfo *pRndSong = SongRndList;
        for(i=0; i<cnt; i++)
        {
            printf("dump the tracks[%d]=%s\n", i, tracks_list[i]);
            apGetSongBasic(tracks_list[i], pRndSong);
            apGetSongUrl(tracks_list[i], pRndSong);
            pRndSong++;
        }
   /// }
    result = XIAMI_MAX_RND_NUM;
    return SongRndList;
}

SongInfo  * DataManager:: get_bills_list(int category, int sub_bill, int &result)
{
    return get_rnd_list(result);
}

SongInfo  * DataManager:: get_radio_list(int channel, int index, int &result)
{    
    const char fm_radio[] = "http://restful.airplayme.com/radios/%s/play.json";  
    const char *pRadioID;
    char url[128] ={0};
    int cnt = 0; 
    
    if( index > 2)
        return (SongInfo  *)NULL;
    pRadioID= radio_id[index];
    memset(url, 0, 128);    
    sprintf(url, fm_radio, pRadioID);

    printf("get_radio_list: url=%s\n", url);
    RadioIndex = (unsigned  int)index;
    char *pReadBuf;
    GetDataFromUrl(url, &pReadBuf);
    apParseRadioTracks(pReadBuf); 

    free((void *)pReadBuf);    
    SongInfo *pRndSong = SongRndList;
    int i;
    cnt = XIAMI_MAX_RND_NUM;
    for(i=0; i<cnt; i++)
    {
        printf("dump the tracks[%d]=%s\n", i, tracks_list[i]);
        apGetSongBasic(tracks_list[i], pRndSong);
        
        //apGetSongUrl(tracks_list[i], pRndSong);
        pRndSong++;
    }
    result = XIAMI_MAX_RND_NUM;
    return SongRndList;    
}

SongInfo  *DataManager::get_my_songs(int page, unsigned int &cnt)
{
    bool ret = false;
    
    ret = GetFavoriteInfo(page, MY_SOGNS);
    if(ret) {
        cnt = retSongNum;
        return MySongs;
    }
    return (SongInfo  *)NULL;
}

SongInfo *DataManager::get_my_albums(int page, unsigned int &cnt)
{
    bool ret = false;
    
    ret = GetFavoriteInfo(page, MY_ALBUMS);
    if(ret){
        cnt = retAlbumNum;
        return apMyAlbums;
    }
    return (SongInfo*)NULL;
}

SongInfo * DataManager::get_my_artists(int page, unsigned int &cnt)
{
    bool ret = false;
    
    ret = GetFavoriteInfo(page, MY_ARTISTS);
    if(ret){
        cnt = retArtistNum;
        return MyArtists;
    }
    return (SongInfo *)NULL;
}

SongInfo * DataManager::get_my_radios(int page, unsigned int &cnt)
{
    bool ret = false;
    
    ret = GetFavoriteInfo(page, MY_RADIOS);
    if(ret){
        cnt = retMyRadioNum;
        return MyRadios;
    }
    return (SongInfo *)NULL;
}

SongInfo *DataManager::get_album_songs(const char *palbums_id,  unsigned int &cnt)
{
    const char apAlbumDetail[] ="http://restful.airplayme.com/albums/%s/detail.json";
    char url[256] ={0};
    bool ret = false;
    char *pBuf;
    
    memset(url, 0, 256);
    sprintf(url, apAlbumDetail, palbums_id);
    ret = GetDataFromUrl(url, &pBuf);
    if(ret) {
        apParseLovedSongs(pBuf, MySongs);
        cnt = retSongNum;
        if(pBuf)
            free((void *)pBuf);
        return MySongs;
    }
    return (SongInfo  *)NULL;    
}

pAlbumInfo DataManager::get_artist_albums(const char *partist_id, unsigned int &cnt)
{
    const char artist_detail[] = "http://restful.airplayme.com/artists/%s/detail.json";
    char url[256] ={0};
    bool ret = false;
    char *pBuf;

    
    sprintf(url, artist_detail, partist_id);
    ret = GetDataFromUrl(url, &pBuf);
    printf("get albums=%s\n", url);
    if(ret){
        pAlbumInfo pAlbums;
        pAlbums = apParseArtistAlbums(pBuf, cnt);
        free((void *)pBuf);
        return  pAlbums;
    }
    cnt = 0;
    return (pAlbumInfo)NULL;
}

SongInfo *DataManager::search_songs(const char *pKeyWord, unsigned int page, unsigned int &cnt)
{
#if 0
    const char search_url[] = "http://restful.airplayme.com/tracks/search.json?q=%s&start=0&offset=1";
    char url[256] ={0};
    bool ret = false;

    memset(url, 0, 256);
    if(!pKeyWord || cnt <0)
        return NULL;
    
    sprintf(url, search_url, pKeyWord, page);

    ret = GetDataFromUrl(url, &DataManager::ParseFavoriteInfo);
    if(ret) {
        cnt = retSongNum;
        return MySongs;
    }
#endif
    
    return (SongInfo  *)NULL;    
}

