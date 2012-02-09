#if !defined(DATAMANAGER_H)
#define DATAMANAGER_H

#define MAX_NAME_LENGHT 256
#include <string.h>
//#include "curl/curl.h"
#include <iostream>

#include "./curl-7.21.7/include/curl/curl.h"

struct MemoryStruct {
  char *memory;
  size_t size;
};

// turn on /off curl verbose info, 0 --off 1 --- on
#define AP_CURLOPT_VERBOSE_DEBUG 0

// function macro define
//#define LYRIC_DL_SUPPORT

#define TRACK_ID_LEN 34
#define TRACK_ID_BUFLEN 35
#define TRACK_MXA_NUM 500

#define MAX_ONE_PAGE_SIZE 7

// const varible define
#define XIAMI_MAX_RND_NUM 14

#define XIAMI_BILLS_NUM 1
#define XIAMI_BILL_SONG_NUM 20

#define XIAMI_RADIO_NUM 2
#define XIAMI_RADIO_SONG_NUM 10

#define XIAMI_MY_SONGS_PAGE 25

#define HTTP_CONNECTION_TIMEOUT 10 // secends
enum{
    FILE_TYPE_MP3 = 0,
    FILE_TYPE_LOGO,
    FILE_TYPE_LYRIC
};

enum{
    MY_SOGNS =0,
    MY_FAV_COLLECTS,        
    MY_COLLECTS,
    MY_ALBUMS,
    MY_ARTISTS,
    MY_RADIOS
};

enum{
    COLLECT_TYPE_SONG=0,
    COLLECT_TYPE_ALBUM,
    COLLECT_TYPE_ARTIST,
    COLLECT_TYPE_RADIO
};

typedef struct Song{
    char track_id[35];
    unsigned int size;
    const char *name;
    const char *album_title;
    const char *lyric;
    const char *singers;
    const char *album_logo;
    const char *location;
}SongInfo;
typedef struct Song *pSongInfo;

typedef struct Album{
    char album_title[128];
    char album_logo[128];
    unsigned int songs_cnt;
    char album_id[TRACK_ID_BUFLEN];
    pSongInfo pAlbumSongs;
}AlbumInfo;
typedef struct Album *pAlbumInfo;
    
typedef struct Artist{
    const char singers[64];
    const char image[128];
    pAlbumInfo pArtistAlbums;
    char artist_id[TRACK_ID_BUFLEN];
}ArtistInfo;
typedef struct Artist *pArtistInfo;

typedef struct Radio{
    const char *name;
    unsigned int albums_cnt;
    char artist_id[TRACK_ID_BUFLEN];
}RadioInfo;
typedef struct Radio *pRadioInfo;

class DataManager
{
    private:
        DataManager();
        ~DataManager();

        static DataManager *m_instance;

        static CURL *mCurl;
        static CURL *pLogoCurl;
	 static bool start_dl_flag;
        static bool restart_dl_flag;
        static SongInfo *pDownList;
        
        char *user_id;
        char *password;
        bool mLogged;
        
        static size_t  write_file(void *ptr, size_t size, size_t nmemb, FILE *stream);
        static size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data);
        // music info parse function
         int	ParseLoginInfo(struct MemoryStruct *pJson);
			
        // for airplay api
          int  apParseRadioTracks(char *pBuf);
          int  apParseSongBasic(char *pBuf, SongInfo *pSong);
          int  apParseSongPlay(char *pBuf, SongInfo *pSong);
          
          int apParseLovedSongs(char *pBuf, SongInfo *pSong);
          int  apParseLovedAlbums(char *pBuf, SongInfo *pSong);
          int  apParseLovedArtists(char *pBuf, SongInfo *pSong);
          int  apParseLovedRadios(char *pBuf, SongInfo *pSong);

        pAlbumInfo apParseArtistAlbums(char *pBuf, unsigned int &size);
        
        bool apGetSongBasic( const char *pTrackID, SongInfo *pSong);
        bool  apParseAlbumImages(char *pBuf, char *pImages);
        
        bool  GetDataFromUrl(const char *pUrl, char **pBuf);

        bool GetFavoriteInfo(const int pageNum, int type);
        int ping_xiami(const char *ifname);
          
        void * do_dl_thread( void * parameter);
        bool  downloadSong(const char *pFileName,  const char *pUrl, int  file_type);

        SongInfo SongRndList[XIAMI_MAX_RND_NUM];  
        SongInfo MySongs[XIAMI_MY_SONGS_PAGE];
        AlbumInfo MyAlbums[XIAMI_MY_SONGS_PAGE];
          
        SongInfo apMyAlbums[XIAMI_MY_SONGS_PAGE];
        SongInfo MyArtists[XIAMI_MY_SONGS_PAGE];
        SongInfo MyRadios[XIAMI_MY_SONGS_PAGE];
        AlbumInfo albums_list[XIAMI_MY_SONGS_PAGE];
        
        static unsigned  int RadioIndex;
        static unsigned int retSongNum;
        static unsigned int retAlbumNum;
        static unsigned int retArtistNum;
        static unsigned int retMyRadioNum;
    public:
        static DataManager *getInstance();
        static void releaseInstance();
        
        bool Login(const char *account, const char*passwd);
        
        SongInfo *get_rnd_list(int &result); 
        // @category --- main bill index, range 0-2
        // @sub_bill  --- sub bill index, range 0
        // return  --- return the a  the song list pointer, such as the rnd list
        SongInfo  * get_bills_list(int category, int sub_bill, int &result);
        SongInfo  * get_radio_list(int channel, int index, int &result);

        SongInfo  *get_my_songs(int page, unsigned int &cnt);
        SongInfo *get_my_albums(int page, unsigned int &cnt);        
        SongInfo *get_my_artists(int page, unsigned int &cnt);
        SongInfo *get_my_radios(int page, unsigned int &cnt);

        SongInfo *get_album_songs(const char *palbums_id, unsigned int &cnt);
        pAlbumInfo get_artist_albums(const char *partist_id,  unsigned int &cnt);
        // search songs by key word
        //@ key_word    key word you want to search
        //@page      the return result's page number
        //@cnt    the song's count searched
        SongInfo *search_songs(const char *pKeyWord, unsigned int page, unsigned int &cnt);

        int    apGetSongUrl( const char *pTrackID, SongInfo *pSong);
        bool apGetAlbumLogo(const char *pAlbumId, char * pAlbumImage);
        
        // for download  files from url
        bool downUrl( const char *pFileName,  const char *pUrl);
};

#endif

