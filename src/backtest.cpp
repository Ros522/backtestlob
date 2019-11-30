// backtest.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

struct Order {
	int side;
	float size;
	float price;
};

class BackTestEnv {
private:

	Order position = { -1,0,0 };
	std::map<long,Order> orders;
	long seq = 0;

public:
	BackTestEnv() {
	}

	float get_position_side() {
		return this->position.side;
	}
	float get_position_size() {
		return this->position.size;
	}
	float get_position_price() {
		return this->position.price;
	}
	std::map<long, Order> get_orders() {
		return orders;
	}

	float add_position(Order newpos) {
		float profit = 0;
		if (this->position.side >= 0) {
			if (newpos.side == this->position.side) {
				float sum_size = this->position.size + newpos.size;
				float newprice = (this->position.size * this->position.price + newpos.size*newpos.price) / sum_size;

				Order newposion = { newpos.side, sum_size, newprice };
				this->position = newposion;
			}
			else if (newpos.side == 0 && this->position.side == 1) {
				float after_size = this->position.size - newpos.size;

				float ex_size = this->position.size - std::max(0.0f, after_size);

				profit = (this->position.price - newpos.price)*ex_size;

				if (after_size > 0.0f) {
					Order newposion = { 1,after_size,this->position.price };
					this->position = newposion;
				}
				else if (after_size < 0.0f) {
					Order newposion = { 0,std::abs(after_size),newpos.price };
					this->position = newposion;
				}
				else {
					this->position = { -1,0,0 };
				}
			}
			else if (newpos.side == 1 && this->position.side == 0) {

				float after_size = this->position.size - newpos.size;

				float ex_size = this->position.size - std::max(0.0f, after_size);

				profit = (newpos.price- this->position.price)*ex_size;

				if (after_size > 0.0f) {
					Order newposion = { 0,after_size,this->position.price };
					this->position = newposion;
				}
				else if (after_size < 0.0f) {
					Order newposion = { 1,std::abs(after_size),newpos.price };
					this->position = newposion;
				}
				else {
					this->position = { -1,0,0 };
				}
			}
		}
		else
		{
			this->position = newpos;
		}
		return profit;
	}

	std::tuple<float, int> step(float low, float high) {
		std::map<long,Order>::iterator it = this->orders.begin();
		int trade = 0;
		float profit = 0;
		while(it != this->orders.end()) {
			//約定チェック
			Order o = it->second;

			//売り注文
			if (o.side == 1 && o.price < high) {
				//約定している
				trade++;
				profit += this->add_position(o);
				it = this->orders.erase(it);
			}
			else if(o.side == 0 && o.price > low){
				//約定している
				trade++;
				profit += this->add_position(o);
				it = this->orders.erase(it);
			}
			else{
				it++;
			}
		}
		return std::forward_as_tuple(profit, trade);
	}

	std::tuple<float, int> step2(int side, float price) {
		std::map<long, Order>::iterator it = this->orders.begin();
		int trade = 0;
		float profit = 0;
		while (it != this->orders.end()) {
			//約定チェック
			Order o = it->second;
			//買い履歴の場合、売り注文を見る
			if (side == 0 && o.side == 1 && o.price < price) {
				//約定している
				trade++;
				profit += this->add_position(o);
				it = this->orders.erase(it);
			}
			else if (side == 1 && o.side == 0 && o.price > price) {
			//売り履歴の場合、買い注文を見る
				trade++;
				profit += this->add_position(o);
				it = this->orders.erase(it);
			}
			else
			{
				it++;
			}
		}
		return std::forward_as_tuple(profit, trade);
	}

	long entry(int side, double size, float price) {
		Order o = { side,size,price };
		this->seq++;
		orders[this->seq] = o;
		return this->seq;
	};

	int cancel(long id) {
		if (orders.count(id) == 0) {
			return -1;
		}
		orders.erase(id);
		return 0;
	};
	void cancelAll() {
		orders.clear();
	};
};

PYBIND11_MODULE(backtestlob, m) {
	m.doc() = "backtestlob";

	py::class_<Order>(m, "Order")
		.def(py::init())
		.def_readwrite("side", &Order::side)
		.def_readwrite("size", &Order::size)
		.def_readwrite("price", &Order::price);

	py::class_<BackTestEnv>(m, "BackTestEnv")
		.def(py::init())
		.def("getPositionSide", &BackTestEnv::get_position_side)
		.def("getPositionSize", &BackTestEnv::get_position_size)
		.def("getPositionPrice", &BackTestEnv::get_position_price)
		.def("getOrders", &BackTestEnv::get_orders)
		.def("step", &BackTestEnv::step)
		.def("step2", &BackTestEnv::step2)
		.def("entry", &BackTestEnv::entry)
		.def("cancel", &BackTestEnv::cancel)
		.def("cancelAll", &BackTestEnv::cancelAll)
		;
}