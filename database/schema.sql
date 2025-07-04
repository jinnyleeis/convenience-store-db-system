/*────────────── 0. RESET ──────────────*/
DROP DATABASE IF EXISTS store;
CREATE DATABASE store;
USE store;

/*────────────── 1. 주요 엔터티 ──────────────*/
CREATE TABLE vendor (
  vendor_id   INT          PRIMARY KEY COMMENT '공급사 PK',
  vendor_name VARCHAR(60)  NOT NULL UNIQUE COMMENT '공급사 상호(유일)',
  phone       VARCHAR(20)  NULL,
  address     VARCHAR(120) NULL
) COMMENT='1차 공급사';

CREATE TABLE product (
  product_upc  BIGINT UNSIGNED PRIMARY KEY COMMENT '전국 공통 바코드',
  name         VARCHAR(80) NOT NULL COMMENT '상품명',
  brand        VARCHAR(40) NULL  COMMENT '브랜드',
  size         VARCHAR(30) NULL  COMMENT '규격·용량(표기)',
  package_type VARCHAR(30) NULL  COMMENT '포장 형태',
  price        DECIMAL(9,2) NOT NULL CHECK (price > 0) COMMENT '소비자가',
  vendor_id    INT NOT NULL,
  FOREIGN KEY (vendor_id) REFERENCES vendor(vendor_id)

) COMMENT='상품(전국 공통)';

CREATE INDEX idx_product_brand ON product(brand);

CREATE TABLE store (
  store_id       INT PRIMARY KEY COMMENT '점포 코드',
  name           VARCHAR(60) NOT NULL UNIQUE COMMENT '점포 명칭(유일)',
  address        VARCHAR(120) NULL COMMENT '점포 주소',
  ownership_type ENUM('FRANCHISE','CORPORATE') NOT NULL COMMENT '소유 형태',
  open_time      TIME NULL COMMENT '영업 시작 시각 (open_time = close_time → 24시간 영업)',
  close_time     TIME NULL COMMENT '영업 종료 시각'
) COMMENT='편의점 매장';

CREATE TABLE customer (
  customer_id    INT PRIMARY KEY COMMENT '회원 PK',
  name           VARCHAR(60) NULL,
  phone          VARCHAR(20) NULL,
  email          VARCHAR(80) NULL UNIQUE,
  loyalty_status ENUM('NONE','SILVER','GOLD','VIP') DEFAULT 'NONE'
) COMMENT='고객(선택 입력)';

CREATE TABLE sale (
  sale_id        BIGINT           PRIMARY KEY COMMENT '영수증 단위 전역 PK',
  date_time      DATETIME         NOT NULL,
  payment_method ENUM('CASH','CARD','ETC') NOT NULL,
  store_id       INT              NOT NULL,

  CONSTRAINT fk_sale_store
    FOREIGN KEY (store_id) REFERENCES store(store_id)
      ON UPDATE CASCADE ON DELETE RESTRICT
) COMMENT='판매 영수증';

CREATE INDEX idx_sale_store_dt ON sale(store_id, date_time);

/*────────────── 2. 관계 테이블 ──────────────*/

/*────────── Customer–Sale 관계 (1:N, 양쪽 모두 partial) ──────────*/
CREATE TABLE sale_customer (
  sale_id    BIGINT PRIMARY KEY COMMENT '영수증 PK(필수‧유일)',
  customer_id INT    NOT NULL COMMENT '고객 PK',

  FOREIGN KEY (sale_id)    REFERENCES sale(sale_id)    ON DELETE CASCADE,
  FOREIGN KEY (customer_id)REFERENCES customer(customer_id)
                              ON UPDATE CASCADE ON DELETE RESTRICT
) COMMENT='회원구매 매핑 (회원인 경우에만 행이 존재)';

CREATE TABLE stock (
  product_upc       BIGINT UNSIGNED,
  store_id          INT,
  inventory_level   INT  NOT NULL CHECK (inventory_level >= 0),
  reorder_threshold INT  NULL CHECK (reorder_threshold >= 0),
  reorder_quantity  INT  NULL CHECK (reorder_quantity  >  0),
  PRIMARY KEY(product_upc, store_id),
  CONSTRAINT fk_stock_product FOREIGN KEY (product_upc) REFERENCES product(product_upc),
  CONSTRAINT fk_stock_store   FOREIGN KEY (store_id)    REFERENCES store(store_id)
) COMMENT='점포-상품별 재고';

CREATE INDEX idx_stock_threshold ON stock(store_id, inventory_level, reorder_threshold);

CREATE TABLE order_list (          -- 발주 이력
  order_id        BIGINT,
  product_upc     BIGINT UNSIGNED,
  store_id        INT,
  order_datetime  DATETIME NOT NULL,
  order_quantity  INT NOT NULL CHECK (order_quantity > 0),
  PRIMARY KEY(order_id, product_upc, store_id),
  CONSTRAINT fk_order_stock FOREIGN KEY (product_upc, store_id)
     REFERENCES stock(product_upc, store_id)
       ON DELETE CASCADE
) COMMENT='재고 발주 기록(시계열)';

CREATE TABLE sale_line_item (      -- 영수증 구성품
  sale_id     BIGINT,
  product_upc BIGINT UNSIGNED,
  quantity    INT NOT NULL CHECK (quantity > 0),
  PRIMARY KEY (sale_id, product_upc),
  CONSTRAINT fk_sli_sale    FOREIGN KEY (sale_id)     REFERENCES sale(sale_id)   ON DELETE CASCADE,
  CONSTRAINT fk_sli_product FOREIGN KEY (product_upc) REFERENCES product(product_upc)
) COMMENT='영수증-상품 상세';

CREATE INDEX idx_sli_product ON sale_line_item(product_upc);

/*────────────── 3. 트리거 ──────────────*/
DELIMITER $$

/*------------------------------------------------------------------
  (1) 재고 차감
------------------------------------------------------------------*/
DROP TRIGGER IF EXISTS trg_stock_deduct $$
CREATE TRIGGER trg_stock_deduct
BEFORE INSERT ON sale_line_item
FOR EACH ROW
BEGIN
  DECLARE cur_stock INT;

  SELECT sk.inventory_level
    INTO cur_stock
    FROM stock sk
    JOIN sale  s ON s.sale_id = NEW.sale_id
   WHERE sk.product_upc = NEW.product_upc
     AND sk.store_id    = s.store_id
   FOR UPDATE;

  IF cur_stock IS NULL THEN
       SIGNAL SQLSTATE '45000'
       SET MESSAGE_TEXT = '해당 점포에 재고 기록이 없습니다.';
  ELSEIF cur_stock < NEW.quantity THEN
       SIGNAL SQLSTATE '45000'
       SET MESSAGE_TEXT = '재고 부족';
  ELSE
       UPDATE stock sk
         JOIN sale s ON s.sale_id = NEW.sale_id
          SET sk.inventory_level = sk.inventory_level - NEW.quantity
        WHERE sk.product_upc = NEW.product_upc
          AND sk.store_id    = s.store_id;
  END IF;
END$$


/*------------------------------------------------------------------
  (2) 마지막 품목 삭제 금지  ─ BEFORE DELETE 
------------------------------------------------------------------*/
DROP TRIGGER IF EXISTS trg_sli_last_del;
DELIMITER $$

CREATE TRIGGER trg_sli_last_del
BEFORE DELETE ON sale_line_item
FOR EACH ROW
BEGIN
    DECLARE cnt INT;

    /* 현재 행까지 포함해 몇 줄 있는지 확인 */
    SELECT COUNT(*)
      INTO cnt
      FROM sale_line_item
     WHERE sale_id = OLD.sale_id;

    /* cnt = 1 이면 ‘마지막 줄’ → 삭제 차단 */
    IF cnt = 1 THEN
        SIGNAL SQLSTATE '45000'
          SET MESSAGE_TEXT =
          'Sale 은 최소 1개의 line item 을 가져야 합니다 – 마지막 품목 삭제 불가';
    END IF;
END$$



/*------------------------------------------------------------------
  (3) 수량을 0·음수로 만드는 UPDATE 금지
------------------------------------------------------------------*/
DROP TRIGGER IF EXISTS trg_sli_last_zero $$
CREATE TRIGGER trg_sli_last_zero
BEFORE UPDATE ON sale_line_item
FOR EACH ROW
BEGIN
  DECLARE cnt INT;

  SELECT COUNT(*) INTO cnt
    FROM sale_line_item
   WHERE sale_id = OLD.sale_id;

  IF cnt = 1 AND NEW.quantity <= 0 THEN
       SIGNAL SQLSTATE '45000'
       SET MESSAGE_TEXT = 'Sale 은 최소 1개의 line item 을 가져야 합니다 – 수량 0 불가';
  END IF;
END$$

DELIMITER ;

