# Changelog

## 0.1.0

First version: an HTTP and HTTPS client on top of libcurl, in the types of `Zevic.Http`.

- `Curl.Get`, `Post`, `Send` and `GetBytes`, and `Version` and `HasHttps`.
- `Curl.Client`, which keeps its connections and has the options `TimeoutMs`, `ConnectTimeoutMs`, `FollowRedirects`, `MaxRedirects`, `UserAgent`, `Insecure`, `CaFile` and `MaxBodyBytes`.
- `Client.Fetch` gives a `Reply` with the body as bytes.
- Redirects, compressed answers (gzip, deflate, br) and proxies (`HTTPS_PROXY`, `NO_PROXY`) work as in the `curl` command.
- Only `http://` and `https://` addresses are accepted. A line break in a header or in a method is refused.
- Errors have the kinds `Timeout`, `Invalid`, `OutOfRange`, `Io` and `Other`.
- The example `examples/fetch`.
