from locust import HttpUser, TaskSet, task, between
import random
import string

class UserBehavior(TaskSet):
    @task
    def search(self):
        query = self.generate_random_query()
        
        endpoint = "/search"
        params = {'q': query}
        
        # Send the GET request
        with self.client.get(endpoint, params=params, catch_response=True) as response:
            if response.status_code == 200:
                try:
                    json_data = response.json()
                    results = json_data.get("results", [])
                    
                    # Validate the response structure (optional)
                    if not isinstance(results, list):
                        response.failure("Invalid response format: 'results' is not a list.")
                except ValueError:
                    response.failure("Response is not valid JSON.")
            else:
                response.failure(f"Unexpected status code: {response.status_code}")

    def generate_random_query(self, length=8):
        """Generates a random alphanumeric string to use as a search query."""
        letters = string.ascii_letters + string.digits
        return ''.join(random.choice(letters) for _ in range(length))

class WebsiteUser(HttpUser):
    tasks = [UserBehavior]
    wait_time = between(1, 5)  # Wait between 1 to 5 seconds between tasks
