[mysql-proxy]
# proxy-address = :4040
proxy-address = :<MYSQL_PROXY_PORT>

# proxy-backend-addresses = 127.0.0.1:3306
# TODO : is it usefull for multi node ?
proxy-backend-addresses = <MYSQL_HOST>:<MYSQL_PORT>
