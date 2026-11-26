use bytes::Bytes;
use http_body_util::{BodyExt, Full, Limited};
use hyper::body::Incoming;
use hyper::server::conn::http1;
use hyper::service::service_fn;
use hyper::{Method, Request, Response, StatusCode};
use hyper_util::rt::TokioIo;
use std::convert::Infallible;
use tokio::net::TcpListener;

async fn handle(request: Request<Incoming>) -> Result<Response<Full<Bytes>>, Infallible> {
    let response = match (request.method(), request.uri().path()) {
        (&Method::GET, "/fixed") => reply(StatusCode::OK, Bytes::from_static(b"ok")),
        (&Method::GET, path) if path.starts_with("/item/") => {
            reply(StatusCode::OK, Bytes::copy_from_slice(path[6..].as_bytes()))
        }
        (&Method::POST, "/body") => {
            match Limited::new(request.into_body(), 12000).collect().await {
                Ok(body) if body.to_bytes().len() == 4096 => {
                    reply(StatusCode::OK, Bytes::from_static(b"ok"))
                }
                Ok(_) => reply(StatusCode::BAD_REQUEST, Bytes::from_static(b"bad body")),
                Err(_) => reply(StatusCode::PAYLOAD_TOO_LARGE, Bytes::from_static(b"too large")),
            }
        }
        _ => reply(StatusCode::NOT_FOUND, Bytes::from_static(b"not found")),
    };
    Ok(response)
}

fn reply(status: StatusCode, body: Bytes) -> Response<Full<Bytes>> {
    Response::builder()
        .status(status)
        .header("Content-Type", "application/json")
        .header("Content-Length", body.len())
        .body(Full::new(body))
        .expect("static benchmark response")
}

#[tokio::main(flavor = "multi_thread", worker_threads = 2)]
async fn main() -> std::io::Result<()> {
    let port = std::env::args().nth(1).expect("listen port");
    let listener = TcpListener::bind(format!("127.0.0.1:{port}")).await?;
    println!("http://localhost:{}", listener.local_addr()?.port());
    loop {
        let (stream, _) = listener.accept().await?;
        stream.set_nodelay(true)?;
        tokio::spawn(async move {
            let _ = http1::Builder::new()
                .keep_alive(true)
                .auto_date_header(false)
                .max_buf_size(16384)
                .serve_connection(TokioIo::new(stream), service_fn(handle))
                .await;
        });
    }
}
