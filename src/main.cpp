#include <iostream>
#include <iomanip>
#include <array>
#include <mysql/mysql.h>

const char* SERVER   = "localhost";
const char* USER     = "root";
const char* PASSWORD = "1234";
const char* DATABASE = "store";

MYSQL *conn;

/*──────────── 공통 유틸 ────────────*/
void die(const std::string& msg){
    std::cerr << msg << " : " << mysql_error(conn) << '\n';
    mysql_close(conn); exit(1);
}
MYSQL_RES* exec(const std::string& q){
    if(mysql_query(conn,q.c_str())) die("Query fail");
    return mysql_store_result(conn);
}

void print(MYSQL_RES* res){
    int nf = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);

    // 1) 헤더 출력
    for(int i = 0; i < nf; ++i){
        int width = (i == 0 ? 10 : 30);
        std::cout << std::left << std::setw(width) << fields[i].name;
    }
    std::cout << "\n";

    // 2) 각 row 출력
    MYSQL_ROW row;
    while((row = mysql_fetch_row(res))){
        for(int i = 0; i < nf; ++i){
            int width = (i == 0 ? 10 : 30);
            const char* val = row[i] ? row[i] : "NULL";
            std::cout << std::left << std::setw(width) << val;
        }
        std::cout << "\n";
    }

    mysql_free_result(res);
}


/*──────────── 7개 질의 함수 ────────────*/
void q1_productAvailability(){
    std::string key;
    std::cout << "UPC / 키워드 입력 > ";
    std::getline(std::cin,key);

    std::string sql =
        "SELECT st.store_id, st.name AS store, p.product_upc, p.name, "
        "       sk.inventory_level "
        "FROM stock sk "
        "JOIN store   st ON st.store_id = sk.store_id "
        "JOIN product p  ON p.product_upc = sk.product_upc "
        "WHERE p.product_upc = '"+key+"' "
        "   OR p.name  LIKE CONCAT('%','"+key+"','%') "
        "   OR p.brand LIKE CONCAT('%','"+key+"','%');";
    print(exec(sql));
}

void q2_topSellingPastMonth(){
    const std::string sql =
        "WITH product_sales AS (                             \n"
        "  SELECT st.store_id, st.name AS store,             \n"
        "         p.product_upc, p.name,                     \n"
        "         SUM(sli.quantity * p.price) AS revenue     \n"
        "  FROM   sale_line_item sli                         \n"
        "  JOIN   sale    s  ON s.sale_id   = sli.sale_id     \n"
        "  JOIN   product p  ON p.product_upc = sli.product_upc\n"
        "  JOIN   store   st ON st.store_id = s.store_id     \n"
        "  WHERE  s.date_time >= CURRENT_DATE - INTERVAL 1 MONTH\n"
        "  GROUP  BY st.store_id, st.name, p.product_upc, p.name\n"
        ")                                                    \n"
        "SELECT store_id, store, product_upc, name,           \n"
        "       CAST(revenue AS CHAR) AS revenue              \n"
        "FROM (                                               \n"
        "  SELECT ps.*,                                       \n"
        "         ROW_NUMBER() OVER (PARTITION BY store_id    \n"
        "                           ORDER BY revenue DESC) AS r\n"
        "  FROM   product_sales ps                            \n"
        ") AS ranked                                          \n"
        "WHERE r = 1                                          \n"
        "ORDER BY store_id;";
    print(exec(sql));
}


void q3_bestStoreThisQuarter(){
    std::string sql =
        "SELECT st.store_id, st.name, "
        "      SUM(sli.quantity*p.price) AS revenue "
        "FROM sale_line_item sli "
        "JOIN sale    s  ON s.sale_id = sli.sale_id "
        "JOIN product p ON p.product_upc = sli.product_upc "
        "JOIN store   st ON st.store_id = s.store_id "
        "WHERE QUARTER(s.date_time)=QUARTER(CURDATE()) "
        "  AND YEAR(s.date_time)=YEAR(CURDATE()) "
        "GROUP BY st.store_id, st.name "
        "HAVING SUM(sli.quantity*p.price) = ("
        "   SELECT MAX(t.tot) "
        "   FROM ( "
        "       SELECT SUM(sli2.quantity*p2.price) AS tot "
        "       FROM sale_line_item sli2 "
        "       JOIN sale    s2 ON s2.sale_id = sli2.sale_id "
        "       JOIN product p2 ON p2.product_upc = sli2.product_upc "
        "       WHERE QUARTER(s2.date_time)=QUARTER(CURDATE()) "
        "         AND YEAR(s2.date_time)=YEAR(CURDATE()) "
        "       GROUP BY s2.store_id "
        "   ) AS t "
        ");";
    print(exec(sql));
}


void q4_vendorStats(){
    std::string sql =
        "SELECT v.vendor_id, v.vendor_name, "
        "       COUNT(DISTINCT p.product_upc) AS product_types, "
        "       CASE WHEN SUM(sli.quantity) IS NULL THEN 0 "
        "            ELSE SUM(sli.quantity) "
        "       END AS total_units_sold "
        "FROM vendor v "
        "LEFT JOIN product p         ON p.vendor_id    = v.vendor_id "
        "LEFT JOIN sale_line_item sli ON sli.product_upc = p.product_upc "
        "GROUP BY v.vendor_id "
        "ORDER BY product_types DESC;";
    print(exec(sql));
}


void q5_reorderAlert(){
    std::string sql =
        "SELECT st.store_id, st.name AS store, p.name AS product, "
        "       sk.inventory_level, sk.reorder_threshold "
        "FROM stock sk "
        "JOIN store st   ON st.store_id = sk.store_id "
        "JOIN product p  ON p.product_upc = sk.product_upc "
        "WHERE sk.inventory_level < sk.reorder_threshold "
        "ORDER BY st.store_id;";
    print(exec(sql));
}
void q6_loyaltyCoffeeBundle() {
    std::string keyword;
    std::cout << "커피 검색어 (ex. 아메리카노) > ";
    std::getline(std::cin, keyword);

    /* 작은따옴표·% 를 포함해 전체 패턴을 미리 만듬 */
    std::string like_kw = "'%" + keyword + "%'";

    std::string sql =
        "WITH coffee_sales AS ( "
        "  SELECT s.sale_id "
        "    FROM sale s "
        "    JOIN sale_customer sc ON sc.sale_id = s.sale_id "
        "    JOIN customer c       ON c.customer_id = sc.customer_id "
        "    JOIN sale_line_item sli ON sli.sale_id   = s.sale_id "
        "    JOIN product p           ON p.product_upc = sli.product_upc "
        "   WHERE c.loyalty_status IN ('VIP','GOLD','SILVER') "
        "     AND p.name LIKE " + like_kw + " "
        "), bundled AS ( "
        "  SELECT p2.name, SUM(sli2.quantity) AS cnt "
        "    FROM coffee_sales cs "
        "    JOIN sale_line_item sli2 ON sli2.sale_id = cs.sale_id "
        "    JOIN product p2          ON p2.product_upc = sli2.product_upc "
        "   WHERE p2.name NOT LIKE " + like_kw + " "
        "   GROUP BY p2.product_upc "
        ") "
        "SELECT name AS item, cnt "
        "  FROM bundled "
        " ORDER BY cnt DESC "
        " LIMIT 3;";

    print(exec(sql));
}



void q7_franchiseVsCorporateVariety(){
    std::string sql =
        /* ---------- ① 매장-별 취급 상품 가짓수 ---------- */
        "SELECT c1.ownership_type, c1.store_id, c1.kinds "
        "FROM ( "
        "   SELECT st.store_id, st.ownership_type, "
        "          COUNT(DISTINCT sk.product_upc) AS kinds "
        "   FROM stock sk "
        "   JOIN store st ON st.store_id = sk.store_id "
        "   GROUP BY st.store_id, st.ownership_type "
        ") AS c1 "
        /* ---------- ② 소유형태-별 최대 가짓수 ---------- */
        "JOIN ( "
        "   SELECT ownership_type, MAX(kinds) AS max_k "
        "   FROM ( "
        "       SELECT st.ownership_type, st.store_id, "
        "              COUNT(DISTINCT sk.product_upc) AS kinds "
        "       FROM stock sk "
        "       JOIN store st ON st.store_id = sk.store_id "
        "       GROUP BY st.store_id, st.ownership_type "
        "   ) AS t "
        "   GROUP BY ownership_type "
        ") AS c2 "
        "ON c1.ownership_type = c2.ownership_type "
        "AND c1.kinds = c2.max_k;";
    print(exec(sql));
}

static const char* TABLES[] = {
    "store",
    "product",
    "vendor",
    "customer",
    "sale",
    "sale_line_item",
    "stock",
    "order_list"
};
static const int NUM_TABLES = sizeof(TABLES) / sizeof(TABLES[0]);

void verifyMinimums(){
    int counts[NUM_TABLES];      // row 수 저장할 배열

    // 각 테이블의 COUNT(*)를 조회
    for(int i = 0; i < NUM_TABLES; ++i){
        std::string q = "SELECT COUNT(*) FROM " + std::string(TABLES[i]) + ";";
        MYSQL_RES* res = exec(q);
        MYSQL_ROW row = mysql_fetch_row(res);
        counts[i] = atoi(row[0]);
        mysql_free_result(res);
    }

    // 결과 출력
    std::cout << "\n[테이블 레코드 수 요약]\n";
    for(int i = 0; i < NUM_TABLES; ++i){
        std::cout << std::left << std::setw(15)
                  << TABLES[i] << ": " << counts[i] << "\n";
    }
}


/*──────────── 메뉴 & 메인 ────────────*/
void menu(){
    std::cout<<"\n=========== Convenience-Store Queries ===========\n"
             <<"1.Product Availability\n2.Top-Selling Items (1M)\n"
             <<"3.Store Performance (Q)\n4.Vendor Statistics\n"
             <<"5.Reorder Alert\n6.Coffee Bundles\n7.Variety by Ownership\n0.Exit\n> ";
}

int main(){
    conn = mysql_init(nullptr);
    if(!mysql_real_connect(conn,SERVER,USER,PASSWORD,DATABASE,0,nullptr,0))
        die("Connection fail");

    verifyMinimums();

    std::string sel;
    while(true){
        menu(); std::getline(std::cin,sel);
        if(sel=="0") break;
        else if(sel=="1") q1_productAvailability();
        else if(sel=="2") q2_topSellingPastMonth();
        else if(sel=="3") q3_bestStoreThisQuarter();
        else if(sel=="4") q4_vendorStats();
        else if(sel=="5") q5_reorderAlert();
        else if(sel=="6") q6_loyaltyCoffeeBundle();
        else if(sel=="7") q7_franchiseVsCorporateVariety();
        else std::cout<<"잘못 선택\n";
    }
    mysql_close(conn);
    return 0;
}