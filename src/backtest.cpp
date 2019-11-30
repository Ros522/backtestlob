#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

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
		float size;
		float price;
	};

	struct Position {
		Side side;
		float size;
		float price;
	};
private:
	Position position = { Side::UNDEF,0,0 };
	std::map<long,Order> orders;
	long seq = 0;

public:
	BackTestEnv() {
	}

	Side get_position_side() {
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

	float add_position(Order neworder) {
		float profit = 0;
		if (this->position.side >= Side::BUY) {
			if (neworder.side == this->position.side) {
				float sum_size = this->position.size + neworder.size;
				float newprice = (this->position.size * this->position.price + neworder.size*neworder.price) / sum_size;

				Position newposion = { neworder.side, sum_size, newprice };
				this->position = newposion;
			}
			else if (neworder.side == Side::BUY && this->position.side == Side::SELL) {
				float after_size = this->position.size - neworder.size;

				float ex_size = this->position.size - std::max(0.0f, after_size);

				profit = (this->position.price - neworder.price)*ex_size;

				if (after_size > 0.0f) {
					Position newposion = { Side::SELL,after_size,this->position.price };
					this->position = newposion;
				}
				else if (after_size < 0.0f) {
					Position newposion = { Side::BUY,std::abs(after_size),neworder.price };
					this->position = newposion;
				}
				else {
					this->position = { Side::UNDEF,0,0 };
				}
			}
			else if (neworder.side == Side::SELL && this->position.side == Side::BUY) {

				float after_size = this->position.size - neworder.size;

				float ex_size = this->position.size - std::max(0.0f, after_size);

				profit = (neworder.price- this->position.price)*ex_size;

				if (after_size > 0.0f) {
					Position newposion = { Side::BUY,after_size,this->position.price };
					this->position = newposion;
				}
				else if (after_size < 0.0f) {
					Position newposion = { Side::SELL,std::abs(after_size),neworder.price };
					this->position = newposion;
				}
				else {
					this->position = { Side::UNDEF,0,0 };
				}
			}
		}
		else
		{
			// IF SIDE= UNDEF
			this->position = { neworder.side,neworder.size,neworder.price };
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

			switch (o.type) {
				case OrderType::LIMIT:
					//売り注文
					if (o.side == Side::SELL && o.price < high) {
						//約定している
						trade++;
						profit += this->add_position(o);
						it = this->orders.erase(it);
					}
					else if (o.side == Side::BUY && o.price > low) {
						//約定している
						trade++;
						profit += this->add_position(o);
						it = this->orders.erase(it);
					}
					else {
						it++;
					}
					break;
				case OrderType::MARKET:
					// 常に約定している
					trade++;
					// 買いの場合、HIGHで約定させる(売りもまた同様）
					if(o.side == Side::BUY) o.price = high;
					if (o.side == Side::SELL) o.price = low;
					profit += this->add_position(o);
					it = this->orders.erase(it);
					break;
			}
		}
		return std::forward_as_tuple(profit, trade);
	}

	std::tuple<float, int> step_by_tick(Side side, float price) {
		std::map<long, Order>::iterator it = this->orders.begin();
		int trade = 0;
		float profit = 0;
		while (it != this->orders.end()) {
			//約定チェック
			Order o = it->second;
			switch (o.type) {
				case OrderType::LIMIT:
					//買い履歴の場合、売り注文を見る
					if (side == Side::BUY && o.side == Side::SELL && o.price < price) {
						//約定している
						trade++;
						profit += this->add_position(o);
						it = this->orders.erase(it);
					}
					else if (side == Side::SELL && o.side == Side::BUY && o.price > price) {
					//売り履歴の場合、買い注文を見る
						trade++;
						profit += this->add_position(o);
						it = this->orders.erase(it);
					}
					else
					{
						it++;
					}
					break;
				case OrderType::MARKET:
					// 同サイドのTickならば約定していると判定する
					if(o.side == side)
					{
						trade++;
						o.price = price;
						profit += this->add_position(o);
						it = this->orders.erase(it);
					}
					else
					{
						it++;
					}
					break;
			}

		}
		return std::forward_as_tuple(profit, trade);
	}

	long entry(OrderType type ,Side side, float size, float price) {
		Order o = { type,side,size,price };
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
	void cancel_all() {
		orders.clear();
	};
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
		.def(py::init())
		.def_readwrite("side", &BackTestEnv::Order::side)
		.def_readwrite("size", &BackTestEnv::Order::size)
		.def_readwrite("price", &BackTestEnv::Order::price);

	py::class_<BackTestEnv>(m, "BackTestEnv")
		.def(py::init())
		.def_property_readonly("side", &BackTestEnv::get_position_side)
		.def_property_readonly("size", &BackTestEnv::get_position_size)
		.def_property_readonly("price", &BackTestEnv::get_position_price)
		.def("get_orders", &BackTestEnv::get_orders)
		.def("step", &BackTestEnv::step)
		.def("step_by_tick", &BackTestEnv::step_by_tick)
		.def("entry", &BackTestEnv::entry)
		.def("cancel", &BackTestEnv::cancel)
		.def("cancel_all", &BackTestEnv::cancel_all)
		;
}