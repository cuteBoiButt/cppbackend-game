#pragma once

#include "db.h"

#include <condition_variable>
#include <mutex>

#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/result>
#include <pqxx/zview>

namespace postgres {

class ConnectionPool {
    using PoolType = ConnectionPool;
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

public:
    class ConnectionWrapper {
    public:
        ConnectionWrapper(std::shared_ptr<pqxx::connection>&& conn, PoolType& pool) noexcept
            : conn_{std::move(conn)}
            , pool_{&pool} {
        }

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        pqxx::connection& operator*() const&& = delete;

        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }

        ~ConnectionWrapper() {
            if (conn_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }

    private:
        std::shared_ptr<pqxx::connection> conn_;
        PoolType* pool_;
    };

    // ConnectionFactory is a functional object returning std::shared_ptr<pqxx::connection>
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }

    ConnectionWrapper GetConnection();

private:
    void ReturnConnection(ConnectionPtr&& conn);

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};

class UnitOfWorkImpl;

class RetiredDogRepositoryImpl : public model::RetiredDogRepository {
public:
    explicit RetiredDogRepositoryImpl(UnitOfWorkImpl& uow)
        : uow_{uow} {}

    void Save(const model::RetiredDog& retired_dog) override;
    std::vector<model::RetiredDog> FetchRange(int offset, int size) override;
private:
    UnitOfWorkImpl& uow_;
};

class UnitOfWorkImpl : public db::UnitOfWork {
public:
    explicit UnitOfWorkImpl(ConnectionPool::ConnectionWrapper conn)
        : connection_{std::move(conn)}, retired_dogs_{*this} {
    }

    model::RetiredDogRepository& GetRetiredDogs() override {
        return retired_dogs_;
    }

    //ОБЯЗАТЕЛЬНО вызвать одну из этих функций до уничтожения объекта
    void Commit() override;
    void Rollback();

    template <typename... Args>
    auto ExecuteParams(pqxx::zview sql, const Args&... args) {
        try {
            return Transaction().exec_params(sql, args...);
        } catch (const std::exception& e) {
            Rollback();
            throw;
        }
    }

    pqxx::result Execute(pqxx::zview sql);

    //В обязаности конечного пользователя входит закомиитить
    //транзакцию до деструктора
    //мы не можем коммитить тут, ибо коммит может выбросить исключение
    //может надо написать что-то в лог если транзакция не была закомичена?
    ~UnitOfWorkImpl();

private:
    pqxx::work& Transaction();

    ConnectionPool::ConnectionWrapper connection_;
    std::optional<pqxx::work> transaction_;
    RetiredDogRepositoryImpl retired_dogs_;
};

void UnitOfWorkImplDeleter(db::UnitOfWork* ptr);

class DatabaseImpl : public db::Database {
public:
    template <typename ConnectionFactory>
    DatabaseImpl(size_t pool_size, ConnectionFactory&& connection_factory) : pool_{pool_size, std::move(connection_factory)} {
        using pqxx::operator"" _zv;
        UnitOfWorkImpl uow{pool_.GetConnection()};

        uow.Execute(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id UUID CONSTRAINT retired_player_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL,
    score int NOT NULL,
    play_time_ms int NOT NULL
);
)"_zv);

        uow.Execute(R"(
CREATE INDEX IF NOT EXISTS retired_players_idx
    ON retired_players (score DESC, play_time_ms, name);
)"_zv);

        uow.Commit();
    }

    std::unique_ptr<db::UnitOfWork, void(*)(db::UnitOfWork*)> GetUoW() override;

private:
    ConnectionPool pool_;
};

void DatabaseImplDeleter(db::Database* ptr);

template <typename... Args>
std::unique_ptr<db::Database, void(*)(db::Database*)> CreateDatabaseImpl(const Args&... args) {
    return std::unique_ptr<DatabaseImpl, void(*)(db::Database*)>(new DatabaseImpl(args...), DatabaseImplDeleter);
}

}
