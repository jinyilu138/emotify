use actix_web::{get, post, web, App, HttpResponse, HttpServer, Responder};
use dotenv::dotenv;
use rspotify::{
    model::{playlist, Country, Market, PlayContextId, PlaylistId, SearchResult, SearchType}, prelude::*, scopes, AuthCodeSpotify, Credentials, OAuth
};
use std::{sync::Mutex};
use std::sync::Arc;
use open;
use std::time::Instant;

fn map_emotion_to_playlist<'a>(emotion: &str) -> Option<PlaylistId<'a>> {
    let uri = match emotion {
        "happy" => "spotify:playlist:37i9dQZF1EVJSvZp5AOML2", // happy
        "sad" => "spotify:playlist:37i9dQZF1EIg85EO6f7KwU", // sad
        "angry" => "spotify:playlist:37i9dQZF1EVHGWrwldPRtj", // calming
        "neutral" => "spotify:playlist:37i9dQZF1EpT1iXiOFRTI4", // most listened to
        _ => "spotify:playlist:37i9dQZF1EP6YuccBxUcC1", // disgust, fear and surprize map to daylist
    };
    PlaylistId::from_uri(uri).ok()
}

#[get("/")]
async fn index() -> impl Responder {
    HttpResponse::Ok().body("hello")
}

#[derive(Clone)]
struct AppState {
    spotify: Arc<Mutex<AuthCodeSpotify>>,
}

#[get("/login")]
async fn login(data: web::Data<AppState>) -> impl Responder {
    let spotify = match data.spotify.lock() {
        Ok(spotify) => spotify,
        Err(poisoned) => {
            eprintln!("[ERROR] login: Mutex poisoned: {:?}", poisoned);
            return HttpResponse::InternalServerError().body("Spotify client unavailable.");
        }
    };

    let auth_url = match spotify.get_authorize_url(false) {
        Ok(url) => url,
        Err(e) => {
            eprintln!("[ERROR] login: Failed to get auth URL: {:?}", e);
            return HttpResponse::InternalServerError().body("Failed to generate login URL.");
        }
    };

    open::that(&auth_url).expect("Can't open browser");
    HttpResponse::Ok().body("Open the link in your browser to login.")
}

#[get("/callback")]
async fn callback(
    query: web::Query<std::collections::HashMap<String, String>>,
    data: web::Data<AppState>,
) -> impl Responder {
    let code = match query.get("code") {
        Some(code) => code,
        None => return HttpResponse::BadRequest().body("Missing code in callback"),
    };

    let mut spotify = match data.spotify.lock() {
        Ok(client) => client,
        Err(poisoned) => {
            eprintln!("[ERROR] callback: Mutex poisoned: {:?}", poisoned);
            return HttpResponse::InternalServerError().body("Spotify client unavailable.");
        }
    };

    match spotify.request_token(code).await {
        Ok(_) => HttpResponse::Ok().body("Spotify authorization complete!"),
        Err(e) => {
            eprintln!("[ERROR] callback: Token request failed: {:?}", e);
            HttpResponse::InternalServerError().body("Spotify auth failed")
        }
    }
}

#[post("/emotion")]
async fn emotion_handler(req_body: String, data: web::Data<AppState>) -> impl Responder {
    let emotion = req_body.to_lowercase();
    let spotify = match data.spotify.lock() {
        Ok(client) => client,
        Err(poisoned) => {
            eprintln!("[ERROR] emotion_handler: Mutex poisoned: {:?}", poisoned);
            return HttpResponse::InternalServerError().body("Spotify client unavailable.");
        }
    };

    if let Some(playlist_id) = map_emotion_to_playlist(&emotion) {
        let context_uri = PlayContextId::from(playlist_id);
        if let Err(e) = spotify.start_context_playback(context_uri, None, None, None).await {
            eprintln!("Playback error: {:?}", e);
        }
    }
    return HttpResponse::Ok().body("playing .");
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    dotenv().ok();
    env_logger::init();

    let creds = Credentials::from_env().expect("Missing client credentials");
    let oauth = OAuth {
        redirect_uri: std::env::var("RSPOTIFY_REDIRECT_URI").unwrap(),
        scopes: scopes!(
            "user-read-playback-state",
            "user-modify-playback-state",
            "user-read-currently-playing",
            "streaming",
            "playlist-read-private",
            "playlist-read-collaborative",
            "user-read-private"
        ),
        ..Default::default()
    };

    let spotify = AuthCodeSpotify::new(creds, oauth);
    let app_state = web::Data::new(AppState {
        spotify: Arc::new(Mutex::new(spotify)),
    });

    HttpServer::new(move || {
        App::new()
            .app_data(app_state.clone())
            .service(index)
            .service(login)
            .service(callback)
            .service(emotion_handler)
    })
    .bind(("127.0.0.1", 8888))?
    .run()
    .await
}
