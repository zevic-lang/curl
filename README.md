# Curl

An HTTP and HTTPS client for [Zevic](https://github.com/zevic-lang/zevic), on top of [libcurl](https://curl.se/libcurl/).

This is the HTTP client of Zevic (the standard library has no HTTP since 0.6.0). It does HTTPS, redirects, compressed answers (gzip, deflate, br), proxies, timeouts, and one connection for many requests. It has the types of a request and an answer (`Request`, `Response`, `Field`). For a server, see the package `Httpd`.

The public API uses only Zevic types and returns `Result` for anything that can fail. A small C layer (`native/zv_curl.c`) holds everything that is awkward to do from Zevic: the options of libcurl (each has its own type), and the collecting of the answer.

## Install

Needs libcurl and its development files: `apt install libcurl4-openssl-dev` on Debian and Ubuntu; macOS has them. The tests run on Linux, macOS and Windows in CI. On Windows libcurl comes from vcpkg (`vcpkg install curl:x64-windows`); its import library is called `libcurl.lib`, so copy it to `curl.lib` and add its directory to `LIB`, and add the `include` directory to `CPATH`. The two tests that need a self-signed HTTPS server run on Linux only.

```
zevpm get zevic-lang/curl@v0.5.0
```

or in `zevic.toml`:

```
[deps]
Curl = { git = "https://github.com/zevic-lang/curl", tag = "v0.5.0" }
```

## Use

```
module Main

import Zevic.Console
import Curl

fn Run() -> Result<Unit, Error> {
    let r: Curl.Response = Curl.Get("https://registry.npmjs.org/left-pad")?
    Console.Println("${r.Status} ${r.Body.Len()} bytes, ${r.Header("content-type").UnwrapOr("?")}")
    Ok(())
}

fn Main() {
    match (Run()) {
        Ok(_) => {},
        Err(e) => Console.Eprintln("${e.Kind().ToString()}: ${e.Message()}"),
    }
}
```

A 404 or a 500 is a `Response`, not an error. An error is a request that did not get an answer.

A `Client` keeps its connections, so many requests to one server are fast, and it has the options:

```
mut c: Curl.Client = Curl.Client.Open()?
defer c.Close()
c.TimeoutMs = 5000
c.UserAgent = "my-app/1.0"

mut req: Curl.Request = Curl.Request.New("PUT", "https://example.com/items/1")
req.SetHeader("Content-Type", "application/json")
req.SetBody("{\"name\": \"x\"}")
let r: Curl.Response = c.Send(req)?
```

A complete program is in `examples/fetch`: `cd examples/fetch && zevpm run -- https://example.com/`.

## API

| | |
| --- | --- |
| `Curl.Get(url)` `Curl.Post(url, body)` `Curl.Send(request)` | one request with a new client; gives `Result<Curl.Response, Error>` |
| `Curl.GetBytes(url)` | the body as `Array<U8>` |
| `Curl.Version()` `Curl.HasHttps()` | the libcurl that is linked |
| `Curl.Client.Open()` `c.Close()` | a client (closing twice is harmless) |
| `c.Get` `c.Post` `c.Send` `c.GetBytes` | the same, with the connections and options of the client |
| `c.Fetch(request)` | gives a `Reply`: `Status`, `Url` (after redirects), `Headers`, `Body` as bytes, `AsText()`, `Header(name)`, `IsOk()` |

Options of a `Client` (set them before a request):

| | default | |
| --- | --- | --- |
| `TimeoutMs` | 0 | the most time of a whole request (0 = no limit) |
| `ConnectTimeoutMs` | 30000 | the most time to make the connection |
| `FollowRedirects` `MaxRedirects` | true, 10 | |
| `UserAgent` | `zevic-curl/0.5` | |
| `CaFile` | "" | a file of certificates to trust instead of the system's |
| `Insecure` | false | does not check the certificate and the host name. Only for tests and servers you trust |
| `MaxBodyBytes` | 0 | refuses a larger body (0 = no limit) |

The kinds of errors: `Timeout`, `Invalid` (a wrong address, method or header; a protocol that is not http or https), `OutOfRange` (too many redirects, a body over `MaxBodyBytes`), `Io` (cannot connect, TLS, the connection broke) and `Other`.

## What it does for you

- Certificates and host names of HTTPS servers are checked. Only `http://` and `https://` addresses are accepted, also in redirects, so a `file://` address cannot be fetched.
- A line break in the name or the value of a header, or in the method, is refused: libcurl would send it as it is, and it would become another header or another request.
- A request body gets no `Content-Type` unless you set one (libcurl alone would send `application/x-www-form-urlencoded`).
- A `GET` or `HEAD` request cannot have a body.
- The proxy of the environment is used (`HTTPS_PROXY`, `HTTP_PROXY`, `NO_PROXY`), as in the `curl` command.

A `Client` is for one thread at a time. A body can hold any bytes (use `GetBytes` or `Fetch`); `Send` turns it into text and bytes that are not UTF-8 become U+FFFD. Request bodies are text. Header lines folded over more lines (an old style) are ignored.

## Test

```
zevpm test
```

Most tests start a small server (written with `Zevic.Net`) on this machine. Two tests need an HTTPS server with a certificate that no store trusts; they run when these are set, and say that they are skipped when not:

```
CURL_TEST_HTTPS_URL=https://localhost:8443/
CURL_TEST_CA_FILE=/path/to/the/certificate.pem
```

CI does it with `openssl s_server` on Linux (see `.github/workflows/ci.yml`). The tests then check that the certificate is refused without `CaFile`, accepted with it, and accepted with `Insecure`.

On a machine with `HTTPS_PROXY` set, add `127.0.0.1` to `NO_PROXY`, or the tests would go through the proxy.

## Releases

A push to `main` that passes the tests in CI is released by the workflow when `version` in `zevic.toml` has no tag yet: it creates the tag `vX.Y.Z` and a GitHub release whose notes are the `## X.Y.Z` section of `CHANGELOG.md`. To release, raise `version`, add that section, and push to `main`.

## License

Apache-2.0
