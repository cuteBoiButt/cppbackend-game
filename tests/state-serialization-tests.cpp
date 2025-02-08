#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model.h"
#include "../src/model_serialization.h"

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const geom::Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                geom::Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Vector serialization") {
    GIVEN("A vector") {
        const geom::Vec2D v{10, 20};
        WHEN("vector is serialized") {
            output_archive << v;

            THEN("it is equal to vector after serialization") {
                InputArchive input_archive{strm};
                geom::Vec2D restored_vector;
                input_archive >> restored_vector;
                CHECK(v == restored_vector);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
        const auto dog = [] {
            Dog dog{"Pluto"s, {42.2, 12.5}, {}, 3, 42};
            dog.SetScore(42);
            CHECK(dog.TryGrabItem(10, 2));
            dog.SetDir(Direction::EAST);
            dog.SetVelocity({2.3, -1.2});
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr{dog};
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetId() == restored.GetId());
                CHECK(dog.GetName() == restored.GetName());
                CHECK(dog.GetPos() == restored.GetPos());
                CHECK(dog.GetVelocity() == restored.GetVelocity());
                CHECK(dog.GetBagCapacity() == restored.GetBagCapacity());
                CHECK(dog.GetBag() == restored.GetBag());
            }
        }
    }
}
