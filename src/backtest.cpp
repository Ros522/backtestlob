#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cmath>
#include <map>
#include <tuple>
#include <cstdlib>
#include <cstdint>

namespace py = pybind11;

class BackTestEnv {
public:
    enum Side {
        UNDEF = -1,
        BUY = 0,
        SELL = 1
    };

    enum OrderType {
        LIMIT = 0,
        MARKET = 1
    };

    struct Order {
        OrderType type;
        Side side;
        long long size;   // 内部整数（サイズ用スケーリング）
        long long price;  // 内部整数（価格用スケーリング）
    };

    struct Position {
        Side side;
        long long size;
        long long price;
    };

private:
    Position position = { Side::UNDEF,0,0 };
    std::map<long,Order> orders;
    std::vector<std::tuple<long,Order,long>> pending_orders; 
    long seq = 0;
    long long timestep = 0;     // シミュレーションの時間
    int delay_timestep = 0;     // 遅延ティック数

    int price_scale;
    int size_scale;
    long long price_factor;
    long long size_factor;

    double taker_fee;  // % (例: 0.001 = 0.1%)
    double maker_fee;

public:
    BackTestEnv(int price_scale_=0, int size_scale_=0,
                double taker_fee_=0.0, double maker_fee_=0.0,
                int delay_timestep_=0)
        : price_scale(price_scale_), size_scale(size_scale_),
          taker_fee(taker_fee_), maker_fee(maker_fee_),
          delay_timestep(delay_timestep_) {
        price_factor = 1;
        for (int i=0; i<price_scale; i++) price_factor *= 10;
        size_factor = 1;
        for (int i=0; i<size_scale; i++) size_factor *= 10;
    }

    // 内部 <-> 外部変換
    long long to_internal_price(double v) const { return static_cast<long long>(std::round(v * price_factor)); }
    double to_external_price(long long v) const { return static_cast<double>(v) / price_factor; }

    long long to_internal_size(double v) const { return static_cast<long long>(std::round(v * size_factor)); }
    double to_external_size(long long v) const { return static_cast<double>(v) / size_factor; }

    Side get_position_side() { return this->position.side; }
    double get_position_size() { return to_external_size(this->position.size); }
    double get_position_price() { return to_external_price(this->position.price); }

    std::map<long, Order> get_orders() { return orders; }

    std::vector<std::tuple<long, Order, long>> get_pending_orders() {
        return pending_orders;
    }
    long long add_position(Order neworder, bool is_taker) {
        long long profit = 0;
        if (this->position.side >= Side::BUY) {
            if (neworder.side == this->position.side) {
                long long sum_size = this->position.size + neworder.size;
                long long newprice =
                    (this->position.size * this->position.price +
                     neworder.size * neworder.price) / sum_size;
                this->position = { neworder.side, sum_size, newprice };
            }
            else if (neworder.side == Side::BUY && this->position.side == Side::SELL) {
                long long after_size = this->position.size - neworder.size;
                long long ex_size = this->position.size - std::max(0LL, after_size);
                profit = (this->position.price - neworder.price) * ex_size / size_factor;
                if (after_size > 0) {
                    this->position = { Side::SELL, after_size, this->position.price };
                }
                else if (after_size < 0) {
                    this->position = { Side::BUY, std::llabs(after_size), neworder.price };
                }
                else {
                    this->position = { Side::UNDEF,0,0 };
                }
            }
            else if (neworder.side == Side::SELL && this->position.side == Side::BUY) {
                long long after_size = this->position.size - neworder.size;
                long long ex_size = this->position.size - std::max(0LL, after_size);
                profit = (neworder.price - this->position.price) * ex_size / size_factor;
                if (after_size > 0) {
                    this->position = { Side::BUY, after_size, this->position.price };
                }
                else if (after_size < 0) {
                    this->position = { Side::SELL, std::llabs(after_size), neworder.price };
                }
                else {
                    this->position = { Side::UNDEF,0,0 };
                }
            }
        }
        else {
            this->position = { neworder.side,neworder.size,neworder.price };
        }

        // 手数料計算
        double fee_rate = is_taker ? taker_fee : maker_fee;
        long long notional = neworder.price * neworder.size;  // 内部整数
        double fee = fee_rate * ( (double)notional / (price_factor * size_factor) );
        profit -= (long long)std::llround(fee * price_factor);  // 内部利益に換算

        return profit;
    }

    std::tuple<double, int, std::vector<Order>> step(double low, double high) {
        timestep++;          // 時間を進める
        flush_pending();     // 遅延注文を反映

        auto it = this->orders.begin();
        int trade = 0;
        long long profit = 0;
        std::vector<Order> filled_orders;   // ← 追加

        long long low_i = to_internal_price(low);
        long long high_i = to_internal_price(high);

        while(it != this->orders.end()) {
            Order o = it->second;

            switch (o.type) {
                case OrderType::LIMIT:
                    if (o.side == Side::SELL && o.price < high_i) {
                        trade++;
                        profit += this->add_position(o, false); // maker
                        filled_orders.push_back(o);             // ← 注文そのものを追加
                        it = this->orders.erase(it);
                    }
                    else if (o.side == Side::BUY && o.price > low_i) {
                        trade++;
                        profit += this->add_position(o, false); // maker
                        filled_orders.push_back(o);
                        it = this->orders.erase(it);
                    }
                    else {
                        it++;
                    }
                    break;
                case OrderType::MARKET:
                    trade++;
                    profit += this->add_position(o, true); // taker
                    filled_orders.push_back(o);
                    it = this->orders.erase(it);
                    break;
            }
        }

        return std::make_tuple(to_external_price(profit), trade, filled_orders);
    }

    std::tuple<double, int> step_by_tick(Side side, double price) {
        auto it = this->orders.begin();
        int trade = 0;
        long long profit = 0;
        long long price_i = to_internal_price(price);

        while (it != this->orders.end()) {
            Order o = it->second;
            switch (o.type) {
                case OrderType::LIMIT:
                    if (side == Side::BUY && o.side == Side::SELL && o.price < price_i) {
                        trade++;
                        profit += this->add_position(o, false); // maker
                        it = this->orders.erase(it);
                    }
                    else if (side == Side::SELL && o.side == Side::BUY && o.price > price_i) {
                        trade++;
                        profit += this->add_position(o, false); // maker
                        it = this->orders.erase(it);
                    }
                    else {
                        it++;
                    }
                    break;
                case OrderType::MARKET:
                    if(o.side == side) {
                        trade++;
                        o.price = price_i;
                        profit += this->add_position(o, true); // taker
                        it = this->orders.erase(it);
                    }
                    else {
                        it++;
                    }
                    break;
            }
        }
        return std::make_tuple(to_external_price(profit), trade);
    }

    // --- 新規注文 ---
    long entry(OrderType type ,Side side, double size, double price) {
        Order o = { type, side, to_internal_size(size), to_internal_price(price) };
        this->seq++;
        // すぐには板に載せず、pendingに入れる
        pending_orders.push_back({this->seq, o, timestep});
        return this->seq;
    };

    // --- step処理前に pending → orders へ移す ---
    void flush_pending() {
        auto it = pending_orders.begin();
        while (it != pending_orders.end()) {
            auto [id, o, t] = *it;
            if (timestep - t >= delay_timestep) {
                orders[id] = o;
                it = pending_orders.erase(it);
            } else {
                ++it;
            }
        }
    }
    int cancel(long id) {
        if (orders.count(id) == 0) return -1;
        orders.erase(id);
        return 0;
    };

    void cancel_all() { orders.clear(); };
};

PYBIND11_MODULE(backtestlob, m) {
    m.doc() = "backtestlob";

    py::enum_<BackTestEnv::OrderType>(m, "OrderType")
        .value("LIMIT", BackTestEnv::OrderType::LIMIT)
        .value("MARKET", BackTestEnv::OrderType::MARKET)
        .export_values();

    py::enum_<BackTestEnv::Side>(m, "Side")
        .value("BUY", BackTestEnv::Side::BUY)
        .value("SELL", BackTestEnv::Side::SELL)
        .export_values();

    py::class_<BackTestEnv::Order>(m, "Order")
        .def(py::init<>())
        .def_readwrite("side", &BackTestEnv::Order::side)
        .def_readwrite("size", &BackTestEnv::Order::size)
        .def_readwrite("price", &BackTestEnv::Order::price);

    py::class_<BackTestEnv>(m, "BackTestEnv")
        .def(py::init<int,int,double,double,int>(),
             py::arg("price_scale")=0,
             py::arg("size_scale")=0,
             py::arg("taker_fee")=0.0,
             py::arg("maker_fee")=0.0,
             py::arg("delay_timestep")=0)
        .def_property_readonly("side", &BackTestEnv::get_position_side)
        .def_property_readonly("size", &BackTestEnv::get_position_size)
        .def_property_readonly("price", &BackTestEnv::get_position_price)
        .def("get_orders", &BackTestEnv::get_orders)
        .def("get_pending_orders", &BackTestEnv::get_pending_orders)   // ←追加
        .def("step", &BackTestEnv::step)
        .def("step_by_tick", &BackTestEnv::step_by_tick)
        .def("entry", &BackTestEnv::entry)
        .def("cancel", &BackTestEnv::cancel)
        .def("cancel_all", &BackTestEnv::cancel_all);
}
