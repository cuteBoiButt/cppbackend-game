#include "postgres.h"

namespace postgres {

using pqxx::operator"" _zv;

void RetiredDogRepositoryImpl::Save(const model::RetiredDog& retired_dog) {
    uow_.ExecuteParams(
        R"(
INSERT INTO retired_players (id, name, score, play_time_ms) VALUES ($1, $2, $3, $4);
)"_zv,
        retired_dog.GetId().ToString(), retired_dog.GetName(), retired_dog.GetScore(), retired_dog.GetPlayTime());
}

std::vector<model::RetiredDog> RetiredDogRepositoryImpl::FetchRange(int offset, int size) {
    auto result = uow_.ExecuteParams(
        R"(
SELECT id, name, score, play_time_ms FROM retired_players
ORDER BY score DESC, play_time_ms, name
LIMIT $1 OFFSET $2;
)"_zv,
        size, offset);

    std::vector<model::RetiredDog> retired_dogs;
    for (const auto& row : result) {
        retired_dogs.emplace_back(
            model::RetiredDog{
                model::RetiredDog::Id::FromString(row[0].as<std::string>()),
                row[1].as<std::string>(),
                row[2].as<int>(),
                row[3].as<int>()
            }
        );
    }
    return retired_dogs;
}

void UnitOfWorkImpl::Commit() {
    if (transaction_) {
        try {
            transaction_->commit();
        } catch (const std::exception& e) {
            Rollback();
            throw;
        }
        transaction_.reset();
    }
}

void UnitOfWorkImpl::Rollback() {
    if (transaction_) {
        transaction_->abort();
        transaction_.reset();
    }
}

UnitOfWorkImpl::~UnitOfWorkImpl() {
    Rollback();
}

pqxx::result UnitOfWorkImpl::Execute(pqxx::zview sql) {
    try {
        return Transaction().exec(sql);
    } catch (const std::exception& e) {
        Rollback();
        throw;
    }
}

pqxx::work& UnitOfWorkImpl::Transaction() {
    if (!transaction_) {
        transaction_.emplace(*connection_);
    }
    return *transaction_;
}

void UnitOfWorkImplDeleter(db::UnitOfWork* ptr) {
    delete static_cast<UnitOfWorkImpl*>(ptr);
}

void DatabaseImplDeleter(db::Database* ptr) {
    delete static_cast<DatabaseImpl*>(ptr);
}

ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection() {
    std::unique_lock lock{mutex_};
    // Блокируем текущий поток и ждём, пока cond_var_ не получит уведомление и не освободится
    // хотя бы одно соединение
    cond_var_.wait(lock, [this] {
        return used_connections_ < pool_.size();
    });
    // После выхода из цикла ожидания мьютекс остаётся захваченным

    return {std::move(pool_[used_connections_++]), *this};
}

void ConnectionPool::ReturnConnection(ConnectionPtr&& conn) {
    // Возвращаем соединение обратно в пул
    {
        std::lock_guard lock{mutex_};
        assert(used_connections_ != 0);
        pool_[--used_connections_] = std::move(conn);
    }
    // Уведомляем один из ожидающих потоков об изменении состояния пула
    cond_var_.notify_one();
}

std::unique_ptr<db::UnitOfWork, void(*)(db::UnitOfWork*)> DatabaseImpl::GetUoW() {
    return std::unique_ptr<UnitOfWorkImpl, void(*)(db::UnitOfWork*)>(new UnitOfWorkImpl(pool_.GetConnection()), UnitOfWorkImplDeleter);
}

}
