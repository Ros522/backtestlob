#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cmath>
#include <map>
#include <tuple>
#include <cstdlib>
#include <cstdint>
#include <vector>

namespace py = pybind11;

class BackTestEnv {
public:
    enum Side { UNDEF=-1, BUY=0, SELL=1 };
    enum OrderType { LIMIT=0, MARKET=1 };

    struct Order {
        OrderType type;
        Side side;
        long long size;
        long long price;
        long long ready_seq;
    };
    struct PendingOrder {
        long id;
        Order o;
        long long ready_seq;
    };
    struct PendingCancel {
        long target_id;
        long long ready_seq;
    };
    struct PendingModify {
        long order_id;
        long long ready_seq;
        long long new_size;
        long long new_price;
    };
    struct Position {
        Side side;
        long long size;
        long long price;
    };

private:
    Position position = {UNDEF,0,0};
    std::map<long, Order> orders;
    std::vector<PendingOrder> pending_orders;
    std::vector<PendingCancel> pending_cancels;
    std::vector<long long> pending_cancel_all;
    std::vector<PendingModify> pending_modifies;

    long seq = 0;
    long long timestep = 0;
    int price_scale;
    int size_scale;
    long long price_factor;
    long long size_factor;
    double taker_fee;
    double maker_fee;

public:
    BackTestEnv(int price_scale_=0, int size_scale_=0, double taker_fee_=0.0, double maker_fee_=0.0)
        : price_scale(price_scale_), size_scale(size_scale_), taker_fee(taker_fee_), maker_fee(maker_fee_) {
        price_factor = 1; for(int i=0;i<price_scale;i++) price_factor*=10;
        size_factor = 1; for(int i=0;i<size_scale;i++) size_factor*=10;
    }

    long long to_internal_price(double v) const { return static_cast<long long>(std::round(v*price_factor)); }
    double to_external_price(long long v) const { return static_cast<double>(v)/price_factor; }
    long long to_internal_size(double v) const { return static_cast<long long>(std::round(v*size_factor)); }
    double to_external_size(long long v) const { return static_cast<double>(v)/size_factor; }

    Side get_position_side() { return position.side; }
    double get_position_size() { return to_external_size(position.size); }
    double get_position_price() { return to_external_price(position.price); }

    std::map<long, Order> get_orders() { return orders; }
    std::vector<PendingOrder> get_pending_orders() const { return pending_orders; }

    long long add_position(Order neworder, bool is_taker) {
        long long profit=0;
        if(position.side>=BUY){
            if(neworder.side==position.side){
                long long sum_size=position.size+neworder.size;
                if(sum_size>0){
                    long long newprice=(position.size*position.price+neworder.size*neworder.price)/sum_size;
                    position={neworder.side,sum_size,newprice};
                }else{ position={UNDEF,0,0}; }
            }else if(neworder.side==BUY && position.side==SELL){
                long long after_size=position.size-neworder.size;
                long long ex_size=position.size-std::max(0LL,after_size);
                profit=(position.price-neworder.price)*ex_size/size_factor;
                if(after_size>0){ position={SELL,after_size,position.price}; }
                else if(after_size<0){ position={BUY,std::llabs(after_size),neworder.price}; }
                else{ position={UNDEF,0,0}; }
            }else if(neworder.side==SELL && position.side==BUY){
                long long after_size=position.size-neworder.size;
                long long ex_size=position.size-std::max(0LL,after_size);
                profit=(neworder.price-position.price)*ex_size/size_factor;
                if(after_size>0){ position={BUY,after_size,position.price}; }
                else if(after_size<0){ position={SELL,std::llabs(after_size),neworder.price}; }
                else{ position={UNDEF,0,0}; }
            }
        }else{ position={neworder.side,neworder.size,neworder.price}; }

        double fee_rate=is_taker?taker_fee:maker_fee;
        long long notional=neworder.price*neworder.size;
        double fee=fee_rate*((double)notional/(price_factor*size_factor));
        profit-=(long long)std::llround(fee*price_factor);
        return profit;
    }

    std::tuple<double,int,std::vector<long>> step(double low,double high,long long current_seq){
        timestep++;
        flush_pending(current_seq);

        auto it=orders.begin();
        int trade=0;
        long long profit=0;
        std::vector<long> filled_ids;
        long long low_i=to_internal_price(low);
        long long high_i=to_internal_price(high);

        while(it!=orders.end()){
            long id=it->first;
            Order o=it->second;
            switch(o.type){
                case LIMIT:
                    if(o.side==SELL && o.price<high_i){
                        trade++; profit+=add_position(o,false); filled_ids.push_back(id); it=orders.erase(it);
                    }else if(o.side==BUY && o.price>low_i){
                        trade++; profit+=add_position(o,false); filled_ids.push_back(id); it=orders.erase(it);
                    }else{ it++; } break;
                case MARKET:
                    trade++; profit+=add_position(o,true); filled_ids.push_back(id); it=orders.erase(it); break;
            }
        }
        return std::make_tuple(to_external_price(profit),trade,filled_ids);
    }

    std::tuple<double,int,std::vector<long>> step_by_tick(Side side,double price,long long current_seq){
        timestep++;
        flush_pending(current_seq);

        auto it=orders.begin();
        int trade=0;
        long long profit=0;
        std::vector<long> filled_ids;
        long long price_i=to_internal_price(price);

        while(it!=orders.end()){
            long id=it->first;
            Order o=it->second;
            switch(o.type){
                case LIMIT:
                    if(side==BUY && o.side==SELL && o.price<price_i){
                        trade++; profit+=add_position(o,false); filled_ids.push_back(id); it=orders.erase(it);
                    }else if(side==SELL && o.side==BUY && o.price>price_i){
                        trade++; profit+=add_position(o,false); filled_ids.push_back(id); it=orders.erase(it);
                    }else{ it++; } break;
                case MARKET:
                    trade++; profit+=add_position(o,true); filled_ids.push_back(id); it=orders.erase(it); break;
            }
        }
        return std::make_tuple(to_external_price(profit),trade,filled_ids);
    }

    long entry(OrderType type, Side side, double size, double price, long long ready_seq, long order_id=-1){
        Order o={type,side,to_internal_size(size),to_internal_price(price),ready_seq};
        if(order_id<0) seq++; else seq=order_id;
        pending_orders.push_back({seq,o,ready_seq});
        return seq;
    }

    // --- 正しい modify_order ---
    int modify_order(long order_id, double size, double price, long long ready_seq=-1){
        if(orders.count(order_id)==0) return -1; // 存在チェック

        if(ready_seq<0){
            // 即時修正
            orders[order_id].size=to_internal_size(size);
            orders[order_id].price=to_internal_price(price);
        }else{
            // 遅延修正として pending_modifies に積む
            pending_modifies.push_back({order_id,ready_seq,to_internal_size(size),to_internal_price(price)});
        }
        return 0;
    }

    void flush_pending(long long current_seq){
        // 1) pending_orders
        auto it=pending_orders.begin();
        while(it!=pending_orders.end()){
            if(current_seq>=it->ready_seq){
                orders[it->id]=it->o;
                it=pending_orders.erase(it);
            }else ++it;
        }

        // 2) pending_modifies
        auto itm=pending_modifies.begin();
        while(itm!=pending_modifies.end()){
            if(current_seq>=itm->ready_seq && orders.count(itm->order_id)>0){
                orders[itm->order_id].size=itm->new_size;
                orders[itm->order_id].price=itm->new_price;
                itm=pending_modifies.erase(itm);
            }else ++itm;
        }

        // 3) pending_cancels
        auto itc=pending_cancels.begin();
        while(itc!=pending_cancels.end()){
            if(current_seq>=itc->ready_seq){
                if(orders.count(itc->target_id)>0) orders.erase(itc->target_id);
                itc=pending_cancels.erase(itc);
            }else ++itc;
        }

        // 4) pending_cancel_all
        auto itca=pending_cancel_all.begin();
        while(itca!=pending_cancel_all.end()){
            if(current_seq>=*itca){
                orders.clear();
                itca=pending_cancel_all.erase(itca);
            }else ++itca;
        }
    }

    int cancel(long id,long long ready_seq=-1){
        if(ready_seq<0){
            if(orders.count(id)==0) return -1;
            orders.erase(id);
            return 0;
        }else{
            pending_cancels.push_back({id,ready_seq});
            return 0;
        }
    }

    void cancel_all(long long ready_seq=-1){
        if(ready_seq<0) orders.clear();
        else pending_cancel_all.push_back(ready_seq);
    }
};
PYBIND11_MODULE(backtestlob,m){
    m.doc()="backtestlob";

    py::enum_<BackTestEnv::OrderType>(m,"OrderType")
        .value("LIMIT",BackTestEnv::OrderType::LIMIT)
        .value("MARKET",BackTestEnv::OrderType::MARKET)
        .export_values();

    py::enum_<BackTestEnv::Side>(m,"Side")
        .value("BUY",BackTestEnv::Side::BUY)
        .value("SELL",BackTestEnv::Side::SELL)
        .export_values();

    py::class_<BackTestEnv::Order>(m,"Order")
        .def(py::init<>())
        .def_readwrite("type",&BackTestEnv::Order::type)
        .def_readwrite("side",&BackTestEnv::Order::side)
        .def_readwrite("size",&BackTestEnv::Order::size)
        .def_readwrite("price",&BackTestEnv::Order::price)
        .def_readwrite("ready_seq",&BackTestEnv::Order::ready_seq);

    py::class_<BackTestEnv::PendingOrder>(m,"PendingOrder")
        .def_readonly("id",&BackTestEnv::PendingOrder::id)
        .def_readonly("o",&BackTestEnv::PendingOrder::o)
        .def_readonly("ready_seq",&BackTestEnv::PendingOrder::ready_seq);

    py::class_<BackTestEnv>(m,"BackTestEnv")
        .def(py::init<int,int,double,double>(),
             py::arg("price_scale")=0,
             py::arg("size_scale")=0,
             py::arg("taker_fee")=0.0,
             py::arg("maker_fee")=0.0)
        .def_property_readonly("side",&BackTestEnv::get_position_side)
        .def_property_readonly("size",&BackTestEnv::get_position_size)
        .def_property_readonly("price",&BackTestEnv::get_position_price)
        .def("get_orders",&BackTestEnv::get_orders)
        .def("get_pending_orders",&BackTestEnv::get_pending_orders)
        .def("entry",&BackTestEnv::entry,
             py::arg("type"),py::arg("side"),
             py::arg("size"),py::arg("price"),
             py::arg("ready_seq"),py::arg("order_id")=-1)
        .def("modify_order",&BackTestEnv::modify_order,
             py::arg("order_id"),py::arg("size"),py::arg("price"),
             py::arg("ready_seq")=-1)
        .def("step",&BackTestEnv::step,
             py::arg("low"),py::arg("high"),
             py::arg("current_seq"))
        .def("step_by_tick",&BackTestEnv::step_by_tick,
             py::arg("side"),py::arg("price"),
             py::arg("current_seq"))
        .def("cancel",&BackTestEnv::cancel,
             py::arg("id"),py::arg("ready_seq")=-1)
        .def("cancel_all",&BackTestEnv::cancel_all,
             py::arg("ready_seq")=-1);
}