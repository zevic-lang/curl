# Changelog

## 0.5.0

The version now follows the version of Zevic that it is made for.

- Needs Zevic 0.5 or later: the fields are written with `let` and `mut`.
- `Reply` has `let` fields: `Status`, `Url`, `Headers` and `Body` are set only when the reply is made. Change: you cannot assign them any more.
- The default `UserAgent` is `zevic-curl/0.5`.

## 0.1.0

First version: an HTTP and HTTPS client on top of libcurl, in the types of `Zevic.Http`.

- `Curl.Get`, `Post`, `Send` and `GetBytes`, and `Version` and `HasHttps`.
- `Curl.Client`, which keeps its connections and has the options `TimeoutMs`, `ConnectTimeoutMs`, `FollowRedirects`, `MaxRedirects`, `UserAgent`, `Insecure`, `CaFile` and `MaxBodyBytes`.
- `Client.Fetch` gives a `Reply` with the body as bytes.
- Redirects, compressed answers (gzip, deflate, br) and proxies (`HTTPS_PROXY`, `NO_PROXY`) work as in the `curl` command.
- Only `http://` and `https://` addresses are accepted. A line break in a header or in a method is refused.
- Errors have the kinds `Timeout`, `Invalid`, `OutOfRange`, `Io` and `Other`.
- The example `examples/fetch`.
