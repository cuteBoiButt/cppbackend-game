#pragma once

#include "model.h"

#include <memory>

namespace db {

class UnitOfWork {
public:
    virtual model::RetiredDogRepository& GetRetiredDogs() = 0;
    virtual void Commit() = 0;
protected:
    virtual ~UnitOfWork() = default;
};

class Database {
public:
    virtual std::unique_ptr<UnitOfWork, void(*)(UnitOfWork*)> GetUoW() = 0;
protected:
    virtual ~Database() = default;
};

};