const http = require('http');

const server = http.createServer((req, resp) => {
    console.log("> new request received!");
    console.log("  - URL: ", req.url);
    console.log("  - Header: ", JSON.stringify(req.headers, null, 8));
    switch (req.url) {
        case '/get':
            resp.end("Ok");
            break;
        case '/get_delay2':
            setTimeout(() => {
                console.log("  - send /get_delay2 response");
                resp.end("Ok");
            }, 2500);
            break;
        case '/post':
            req.on('data', function (data) {
                console.log(" - POST DATA: ", data);
            })
            resp.statusCode = 201;
            resp.end("added");
            break;
        case '/put':
            req.on('data', function (data) {
                console.log(" - PUT DATA: ", data);
            })
            resp.end("updated");
            break;
        case '/delete':
            resp.statusCode = 204;
            resp.end();
            break;
        default:
            resp.statusCode = 404;
            resp.end();
            break;
    }
}).listen(3000);
